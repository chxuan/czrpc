#pragma once

#include <vector>
#include <list>
#include <memory>
#include <boost/asio.hpp>
#include <boost/timer.hpp>
#include "base/protocol_define.hpp"
#include "base/atimer.hpp"
#include "base/table/threadsafe_list.hpp"
#include "base/czlog.hpp"

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

    void async_write(const response_content& content)
    {
        unsigned int message_name_len = static_cast<unsigned int>(content.message_name.size());
        unsigned int body_len = static_cast<unsigned int>(content.body.size());
        response_header header{ message_name_len, body_len };
        auto buffer = get_buffer(response_data{ header, content });
        async_write_impl(buffer);
    }

    void async_write(const push_content& content)
    {
        unsigned int protocol_len = static_cast<unsigned int>(content.protocol.size());
        unsigned int message_name_len = static_cast<unsigned int>(content.message_name.size());
        unsigned int body_len = static_cast<unsigned int>(content.body.size());
        push_header header{ protocol_len, message_name_len, body_len };
        auto buffer = get_buffer(push_data{ header, content });
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
            if (!socket_.is_open())
            {
                log_warn() << "Socket is not open";
                handle_error();
                return;
            }
            if (ec)
            {
                handle_error();
                return;
            }

            memcpy(&req_head_, req_head_buf_, sizeof(req_head_buf_));
            read_content();
        });
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
            if (!socket_.is_open())
            {
                log_warn() << "Socket is not open";
                handle_error();
                return;
            }
            if (ec)
            {
                handle_error();
                return;
            }

            auto content = make_content();
            type_ = content.flag.type;
            route_(content, self);
        });
    }

    request_content make_content()
    {
        request_content content;
        memcpy(&content.call_id, &content_[0], sizeof(content.call_id));
        memcpy(&content.flag, &content_[sizeof(content.call_id)], sizeof(content.flag));
        content.protocol.assign(&content_[sizeof(content.call_id) + sizeof(client_flag)], req_head_.protocol_len);
        content.message_name.assign(&content_[sizeof(content.call_id) + sizeof(client_flag) + req_head_.protocol_len], req_head_.message_name_len);
        content.body.assign(&content_[sizeof(content.call_id) + sizeof(client_flag) + req_head_.protocol_len + req_head_.message_name_len], 
                            req_head_.body_len);
        return std::move(content);
    }

    void set_no_delay()
    {
        boost::asio::ip::tcp::no_delay option(true);
        boost::system::error_code ec;
        socket_.set_option(option, ec);
    }

    std::shared_ptr<std::string> get_buffer(const response_data& data)
    {
        auto buffer = std::make_shared<std::string>();
        buffer->append(reinterpret_cast<const char*>(&data.header), sizeof(data.header));
        buffer->append(reinterpret_cast<const char*>(&data.content.call_id), sizeof(data.content.call_id));
        buffer->append(reinterpret_cast<const char*>(&data.content.code), sizeof(data.content.code));
        buffer->append(data.content.message_name);
        buffer->append(data.content.body);
        return buffer;
    }

    std::shared_ptr<std::string> get_buffer(const push_data& data)
    {
        auto buffer = std::make_shared<std::string>();
        buffer->append(reinterpret_cast<const char*>(&data.header), sizeof(data.header));
        buffer->append(reinterpret_cast<const char*>(&data.content.mode), sizeof(data.content.mode));
        buffer->append(data.content.protocol);
        buffer->append(data.content.message_name);
        buffer->append(data.content.body);
        return buffer;
    }

    void async_write_impl(const std::shared_ptr<std::string>& buffer)
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
        boost::asio::async_write(socket_, boost::asio::buffer(*send_queue_.front()), 
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
                send_queue_.clear();
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
    threadsafe_list<std::shared_ptr<std::string>> send_queue_;
    std::string session_id_;

    std::function<void(const std::string&)> client_connect_notify_ = nullptr;
    std::function<void(const std::string&)> client_disconnect_notify_ = nullptr;
};

}
}

