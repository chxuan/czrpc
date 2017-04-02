#pragma once

#include <vector>
#include <list>
#include <memory>
#include <boost/asio.hpp>
#include <boost/timer.hpp>
#include "base/header.hpp"
#include "base/scope_guard.hpp"
#include "base/atimer.hpp"
#include "base/logger.hpp"
#include "base/table/threadsafe_list.hpp"

using namespace czrpc::base;
using namespace czrpc::base::table;

namespace czrpc
{
namespace server
{
class connection;
using connection_ptr = std::shared_ptr<connection>;
using connection_weak_ptr = std::weak_ptr<connection>;
using router_callback = std::function<void(const request_content&, const std::shared_ptr<connection>&)>;
using handle_error_callback = std::function<void(const connection_ptr&)>;

class connection : public std::enable_shared_from_this<connection>
{
public:
    connection() = default;
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
    connection(boost::asio::io_service& ios, 
               const router_callback& route_func, 
               const handle_error_callback& handle_error_func,
               const std::function<void(const std::string&)>& client_connect,
               const std::function<void(const std::string&)>& client_disconnect)
        : ios_(ios), socket_(ios), 
        route_(route_func), handle_error_(handle_error_func),
        client_connect_notify_(client_connect),
        client_disconnect_notify_(client_disconnect){} 

    ~connection()
    {
        disconnect();
    }

    void start()
    {
        client_connect_notify_callback();
        set_no_delay();
        read_head();
    }

    boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    void write(const response_content& content)
    {
        unsigned int message_name_len = static_cast<unsigned int>(content.message_name.size());
        unsigned int body_len = static_cast<unsigned int>(content.body.size());
        if (message_name_len + body_len > max_buffer_len)
        {
            handle_error();
            throw std::runtime_error("Send data is too big");
        }

        response_header header{ message_name_len, body_len };
        std::string buffer = get_buffer(response_data{ header, content });
        write_impl(buffer);
    }

    void write(const push_content& content)
    {
        unsigned int protocol_len = static_cast<unsigned int>(content.protocol.size());
        unsigned int message_name_len = static_cast<unsigned int>(content.message_name.size());
        unsigned int body_len = static_cast<unsigned int>(content.body.size());
        if (protocol_len + message_name_len + body_len > max_buffer_len)
        {
            handle_error();
            throw std::runtime_error("Send data is too big");
        }

        push_header header{ protocol_len, message_name_len, body_len };
        std::string buffer = get_buffer(push_data{ header, content });
        write_impl(buffer);
    }

    void async_write(const response_content& content)
    {
        unsigned int message_name_len = static_cast<unsigned int>(content.message_name.size());
        unsigned int body_len = static_cast<unsigned int>(content.body.size());
        if (message_name_len + body_len > max_buffer_len)
        {
            handle_error();
            throw std::runtime_error("Send data is too big");
        }

        response_header header{ message_name_len, body_len };
        std::string buffer = get_buffer(response_data{ header, content });
        async_write_impl(buffer);
    }

    void async_write(const push_content& content)
    {
        unsigned int protocol_len = static_cast<unsigned int>(content.protocol.size());
        unsigned int message_name_len = static_cast<unsigned int>(content.message_name.size());
        unsigned int body_len = static_cast<unsigned int>(content.body.size());
        if (protocol_len + message_name_len + body_len > max_buffer_len)
        {
            handle_error();
            throw std::runtime_error("Send data is too big");
        }

        push_header header{ protocol_len, message_name_len, body_len };
        std::string buffer = get_buffer(push_data{ header, content });
        async_write_impl(buffer);
    }

    void disconnect()
    {
        if (socket_.is_open())
        {
            boost::system::error_code ignore_ec;
            socket_.shutdown(boost::asio::socket_base::shutdown_both, ignore_ec);
            socket_.close(ignore_ec);
        }
    }

    std::string get_session_id()
    {
        if (session_id_.empty())
        {
            if (socket_.is_open())
            {
                boost::system::error_code ec, ec2;
                auto local_endpoint = socket_.local_endpoint();
                auto remote_endpoint = socket_.remote_endpoint();
                if (!ec && !ec2)
                {
                    session_id_ = local_endpoint.address().to_string() + ":"
                                + std::to_string(local_endpoint.port()) + "#"
                                + remote_endpoint.address().to_string() + ":"
                                + std::to_string(remote_endpoint.port());
                }
            }
        }
        return session_id_;
    }

private:
    void read_head()
    {
        auto self(this->shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(req_head_buf_), 
                                [this, self](boost::system::error_code ec, std::size_t)
        {
            auto guard = make_guard([this, self]{ handle_error(); });
            if (!socket_.is_open())
            {
                log_warn("Socket is not open");
                return;
            }

            if (ec)
            {
                return;
            }

            if (check_head())
            {
                read_content();
            }
            else
            {
                read_head();
            }
            guard.dismiss();
        });
    }

    bool check_head()
    {
        memcpy(&req_head_, req_head_buf_, sizeof(req_head_buf_));
        if (req_head_.protocol_len + req_head_.message_name_len + req_head_.body_len > max_buffer_len)
        {
            log_warn("Content len is too big");
            return false;
        }
        return true;
    }

    void read_content()
    {
        content_.clear();
        content_.resize(sizeof(unsigned int) + sizeof(client_flag) + req_head_.protocol_len + req_head_.message_name_len + req_head_.body_len);
        auto self(this->shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(content_), 
                                [this, self](boost::system::error_code ec, std::size_t)
        {
            read_head();
            auto guard = make_guard([this, self]{ handle_error(); });
            if (!socket_.is_open())
            {
                log_warn("Socket is not open");
                return;
            }

            if (ec)
            {
                return;
            }

            request_content content;
            memcpy(&content.call_id, &content_[0], sizeof(content.call_id));
            memcpy(&content.flag, &content_[sizeof(content.call_id)], sizeof(content.flag));
            content.protocol.assign(&content_[sizeof(content.call_id) + sizeof(client_flag)], req_head_.protocol_len);
            content.message_name.assign(&content_[sizeof(content.call_id) + sizeof(client_flag) + req_head_.protocol_len], req_head_.message_name_len);
            content.body.assign(&content_[sizeof(content.call_id) + sizeof(client_flag) + req_head_.protocol_len + req_head_.message_name_len], 
                                req_head_.body_len);
            type_ = content.flag.type;
            route_(content, self);
            guard.dismiss();
        });
    }

    void set_no_delay()
    {
        boost::asio::ip::tcp::no_delay option(true);
        boost::system::error_code ec;
        socket_.set_option(option, ec);
    }

    std::string get_buffer(const response_data& data)
    {
        std::string buffer;
        buffer.append(reinterpret_cast<const char*>(&data.header), sizeof(data.header));
        buffer.append(reinterpret_cast<const char*>(&data.content.call_id), sizeof(data.content.call_id));
        buffer.append(reinterpret_cast<const char*>(&data.content.code), sizeof(data.content.code));
        buffer.append(data.content.message_name);
        buffer.append(data.content.body);
        return std::move(buffer);
    }

    std::string get_buffer(const push_data& data)
    {
        std::string buffer;
        buffer.append(reinterpret_cast<const char*>(&data.header), sizeof(data.header));
        buffer.append(reinterpret_cast<const char*>(&data.content.mode), sizeof(data.content.mode));
        buffer.append(data.content.protocol);
        buffer.append(data.content.message_name);
        buffer.append(data.content.body);
        return std::move(buffer);
    }

    void write_impl(const std::string& buffer)
    {
        boost::system::error_code ec;
        boost::asio::write(socket_, boost::asio::buffer(buffer), ec);
        if (ec)
        {
            handle_error();
            throw std::runtime_error(ec.message());
        }
    }

    void async_write_impl(const std::string& buffer)
    {
        auto self(this->shared_from_this());
        ios_.post([this, self, buffer]
        {
            bool is_empty = send_queue_.empty();
            send_queue_.emplace_back(buffer);
            if (is_empty)
            {
                async_write_impl();
            }
        });
    }

    void async_write_impl()
    {
        auto self(this->shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(send_queue_.front()), 
                                 [this, self](boost::system::error_code ec, std::size_t)
        {
            if (!ec)
            {
                send_queue_.pop_front();
                if (!send_queue_.empty())
                {
                    async_write_impl();
                }
            }
            else
            {
                log_warn(ec.message());
                send_queue_.clear();
                handle_error();
            }
        });
    }

    void handle_error()
    {
        if (type_ == client_type::sub_client)
        {
            handle_error_(this->shared_from_this());
        }
        client_disconnect_notify_callback();
    }

    void client_connect_notify_callback()
    {
        if (client_connect_notify_ != nullptr)
        {
            std::string session_id = get_session_id();
            if (!session_id.empty())
            {
                client_connect_notify_(session_id);
            }
        }
    }

    void client_disconnect_notify_callback()
    {
        if (client_disconnect_notify_ != nullptr)
        {
            std::string session_id = get_session_id();
            if (!session_id.empty())
            {
                client_disconnect_notify_(session_id);
            }
        }
    }

private:
    boost::asio::io_service& ios_;
    boost::asio::ip::tcp::socket socket_;
    char req_head_buf_[request_header_len];
    request_header req_head_;
    client_type type_;
    std::vector<char> content_;
    router_callback route_;
    handle_error_callback handle_error_;
    threadsafe_list<std::string> send_queue_;
    std::string session_id_;

    std::function<void(const std::string&)> client_connect_notify_ = nullptr;
    std::function<void(const std::string&)> client_disconnect_notify_ = nullptr;
};

}
}

