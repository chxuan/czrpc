#pragma once

#include <vector>
#include <list>
#include <memory>
#include <boost/asio.hpp>
#include <boost/timer.hpp>
#include "base/header.hpp"
#include "base/atimer.hpp"
#include "base/scope_guard.hpp"
#include "base/logger.hpp"
#include "base/async_send_queue.hpp"

using namespace czrpc::base;

namespace czrpc
{
class connection;
using connection_ptr = std::shared_ptr<connection>;
using connection_weak_ptr = std::weak_ptr<connection>;
using router_callback = std::function<bool(const request_content&, const client_flag&, const std::shared_ptr<connection>&)>;
using handle_error_callback = std::function<void(const connection_ptr&)>;

class connection : public std::enable_shared_from_this<connection>
{
public:
    connection() = default;
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
    connection(boost::asio::io_service& ios, 
               std::size_t timeout_milli, const router_callback& route_func, 
               const handle_error_callback& handle_error_func)
        : ios_(ios), socket_(ios), timeout_milli_(timeout_milli), route_(route_func), 
        handle_error_(handle_error_func) {} 

    ~connection()
    {
        disconnect();
    }

    void start()
    {
        set_no_delay();
        read_head();
    }

    boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

#if 0
    void write(const std::string& body, const std::string& call_id = "")
    {
        unsigned int call_id_len = static_cast<unsigned int>(call_id.size());
        unsigned int body_len = static_cast<unsigned int>(body.size());
        if (call_id_len + body_len > max_buffer_len)
        {
            handle_error();
            throw std::runtime_error("Send data is too big");
        }

        std::string buffer = get_buffer(response_header{ call_id_len, body_len }, call_id, body);
        write_impl(buffer);
    }
#endif

    void write(const std::string& protocol, const std::string& body, serialize_mode mode)
    {
        unsigned int protocol_len = static_cast<unsigned int>(protocol.size());
        unsigned int body_len = static_cast<unsigned int>(body.size());
        if (protocol_len + body_len > max_buffer_len)
        {
            handle_error();
            throw std::runtime_error("Send data is too big");
        }

        std::string buffer = get_buffer(push_header{ protocol_len,  body_len, mode }, protocol, body);
        write_impl(buffer);
    }

    void async_write(const response_content& content)
    {
        unsigned int call_id_len = static_cast<unsigned int>(content.call_id.size());
        unsigned int message_name_len = static_cast<unsigned int>(content.message_name.size());
        unsigned int body_len = static_cast<unsigned int>(content.body.size());
        if (call_id_len + message_name_len + body_len > max_buffer_len)
        {
            handle_error();
            throw std::runtime_error("Send data is too big");
        }

        response_header header{ call_id_len, message_name_len, body_len };
        std::string buffer = get_buffer(response_data{ header, content });
        async_write_impl(buffer);
    }

    void async_write(const std::string& protocol, const std::string& body, serialize_mode mode)
    {
        unsigned int protocol_len = static_cast<unsigned int>(protocol.size());
        unsigned int body_len = static_cast<unsigned int>(body.size());
        if (protocol_len + body_len > max_buffer_len)
        {
            handle_error();
            throw std::runtime_error("Send data is too big");
        }

        std::string buffer = get_buffer(push_header{ protocol_len,  body_len, mode }, protocol, body);
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
                log_warn(ec.message());
                return;
            }

            if (check_head())
            {
                read_content();
                guard.dismiss();
            }
        });
    }

    bool check_head()
    {
        memcpy(&req_head_, req_head_buf_, sizeof(req_head_buf_));
        unsigned int len = req_head_.call_id_len + req_head_.protocol_len + req_head_.body_len;
        return (len > 0 && len < max_buffer_len) ? true : false;
    }

    void read_content()
    {
        content_.clear();
        content_.resize(req_head_.call_id_len + req_head_.protocol_len + req_head_.body_len);
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
                log_warn(ec.message());
                return;
            }

            request_content content;
            content.call_id.assign(&content_[0], req_head_.call_id_len);
            content.protocol.assign(&content_[req_head_.call_id_len], req_head_.protocol_len);
            content.message_name.assign(&content_[req_head_.call_id_len + req_head_.protocol_len], req_head_.message_name_len);
            content.body.assign(&content_[req_head_.call_id_len + req_head_.protocol_len + req_head_.message_name_len], 
                                req_head_.body_len);
            bool ok = route_(content, req_head_.flag, self);
            if (!ok)
            {
                log_warn("Router failed");
                return;
            }
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
        buffer.append(data.content.call_id);
        buffer.append(data.content.message_name);
        buffer.append(data.content.body);
        return std::move(buffer);
    }

    std::string get_buffer(const push_header& head, const std::string& protocol, const std::string& body)
    {
        std::string buffer;
        buffer.append(reinterpret_cast<const char*>(&head), sizeof(head));
        buffer.append(protocol);
        buffer.append(body);
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
            std::cout << "size: " << send_queue_.size() << std::endl;
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
        if (req_head_.flag.type == client_type::sub_client)
        {
            handle_error_(this->shared_from_this());
        }
    }

private:
    boost::asio::io_service& ios_;
    boost::asio::ip::tcp::socket socket_;
    char req_head_buf_[request_header_len];
    request_header req_head_;
    std::vector<char> content_;
    std::size_t timeout_milli_ = 0;
    router_callback route_;
    handle_error_callback handle_error_;
    async_send_queue send_queue_;
};

}

