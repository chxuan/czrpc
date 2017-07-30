#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <google/protobuf/message.h>
#include <boost/asio.hpp>
#include "base/protocol_define.hpp"
#include "base/atimer.hpp"
#include "base/scope_guard.hpp"
#include "base/serialize_util.hpp"
#include "base/thread_pool.hpp"
#include "base/table/threadsafe_list.hpp"
#include "base/table/threadsafe_unordered_map.hpp"
#include "base/czlog.hpp"

using namespace czrpc::base;
using namespace czrpc::base::table;

namespace czrpc
{
namespace client
{
class client_base
{
public:
    client_base() : work_(ios_), socket_(ios_) {}
    virtual ~client_base()
    {
        stop();
    }

    client_base& connect(const endpoint& ep)
    {
        boost::asio::ip::tcp::resolver resolver(ios_);
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), ep.ip, std::to_string(ep.port));
        endpoint_iter_ = resolver.resolve(query);
        return *this;
    }

    client_base& timeout(time_t connect_timeout, time_t request_timeout)
    {
        connect_timeout_ = connect_timeout;
        request_timeout_ = request_timeout;
        return *this;
    }

    client_base& resend(bool is_resend)
    {
        is_resend_ = is_resend;
        return *this;
    }

    virtual void run()
    {
        start_ios_thread();
    }

    virtual void stop()
    {
        is_stoped_ = true;
        disconnect();
        stop_ios_thread();
    }

    void set_connect_success_notify(const std::function<void()>& func)
    {
        connect_success_notify_ = func;
    }

    void async_write(const request_content& content)
    {
        request_header header;
        header.protocol_len = content.protocol.size();
        header.message_name_len = content.message_name.size();
        header.body_len = content.body.size();

        auto buffer = get_buffer(request_data{ header, content });
        async_write_impl(buffer);
    }

protected:
    void write(const request_content& content)
    {
        request_header header;
        header.protocol_len = content.protocol.size();
        header.message_name_len = content.message_name.size();
        header.body_len = content.body.size();

        auto buffer = get_buffer(request_data{ header, content });
        write_impl(buffer);
    }

    response_content read()
    {
        read_head();
        memcpy(&rsp_head_, rsp_head_buf_, sizeof(rsp_head_buf_));
        return std::move(read_content());
    }

    void disconnect()
    {
        is_connected_ = false;
        if (socket_.is_open())
        {
            boost::system::error_code ignore_ec;
            socket_.shutdown(boost::asio::socket_base::shutdown_both, ignore_ec);
            socket_.close(ignore_ec);
        }
    }

    boost::asio::ip::tcp::socket& get_socket()
    {
        return socket_;
    }

    bool try_connect()
    {
        if (!is_connected_)
        {
            std::lock_guard<std::mutex> lock(conn_mutex_);
            if (!is_connected_)
            {
                connect();
                is_connected_ = true;
                return true;
            }
        }
        return false;
    }

    response_content make_response_content()
    {
        response_content content;
        memcpy(&content.call_id, &rsp_content_[0], sizeof(content.call_id));
        memcpy(&content.code, &rsp_content_[sizeof(content.call_id)], sizeof(content.code));
        content.message_name.assign(&rsp_content_[sizeof(content.call_id) + sizeof(content.code)], rsp_head_.message_name_len);
        content.body.assign(&rsp_content_[sizeof(content.call_id) + sizeof(content.code) + rsp_head_.message_name_len], rsp_head_.body_len);
        return std::move(content);
    }

private:
    std::shared_ptr<std::string> get_buffer(const request_data& data)
    {
        auto buffer = std::make_shared<std::string>();
        buffer->append(reinterpret_cast<const char*>(&data.header), sizeof(data.header));
        buffer->append(reinterpret_cast<const char*>(&data.content.call_id), sizeof(data.content.call_id));
        buffer->append(reinterpret_cast<const char*>(&data.content.flag), sizeof(data.content.flag));
        buffer->append(data.content.protocol);
        buffer->append(data.content.message_name);
        buffer->append(data.content.body);
        return buffer;
    }

    void connect()
    {
        time_t begin_time = time(nullptr);
        while (true)
        {
            try
            {
                boost::asio::connect(socket_, endpoint_iter_);
                break;
            }
            catch (std::exception& e)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                if (time(nullptr) - begin_time >= connect_timeout_)
                {
                    throw std::runtime_error(e.what());
                }
            }
        }
    }

    void write_impl(const std::shared_ptr<std::string>& buffer)
    {
        boost::system::error_code ec;
        boost::asio::write(socket_, boost::asio::buffer(*buffer), ec);
        if (ec)
        {
            is_connected_ = false;
            throw std::runtime_error(ec.message());
        }
    }

    void async_write_impl(const std::shared_ptr<std::string>& buffer)
    {
        ios_.post([this, buffer]
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
        boost::asio::async_write(socket_, boost::asio::buffer(*send_queue_.front()), 
                                 [this](boost::system::error_code ec, std::size_t)
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
                is_connected_ = false;
                if (is_resend_)
                {
                    if (!send_queue_.empty())
                    {
                        async_write_impl();
                    }
                }
                else
                {
                    send_queue_.clear();
                }
                log_warn() << ec.message();
            }
        });
    }

    void read_head()
    {
        boost::system::error_code ec;
        boost::asio::read(socket_, boost::asio::buffer(rsp_head_buf_), ec);
        if (ec)
        {
            disconnect();
            throw std::runtime_error(ec.message());
        }
    }

    response_content read_content()
    {
        rsp_content_.clear();
        rsp_content_.resize(sizeof(unsigned int) + sizeof(rpc_error_code) + rsp_head_.message_name_len + rsp_head_.body_len);
        boost::system::error_code ec;
        boost::asio::read(socket_, boost::asio::buffer(rsp_content_), ec); 
        if (ec)
        {
            disconnect();
            throw std::runtime_error(ec.message());
        }

        return std::move(make_response_content());
    }


    void start_ios_thread()
    {
        if (thread_ == nullptr)
        {
            thread_ = std::make_unique<std::thread>([this]{ ios_.run(); });
        }
    }

    void stop_ios_thread()
    {
        ios_.stop();
        if (thread_ != nullptr)
        {
            if (thread_->joinable())
            {
                thread_->join();
            }
        }
    }

protected:
    client_type client_type_;
    time_t connect_timeout_ = 3;
    time_t request_timeout_ = 10;
    char rsp_head_buf_[response_header_len];
    response_header rsp_head_;
    std::vector<char> rsp_content_;
    boost::asio::ip::tcp::resolver::iterator endpoint_iter_;
    std::function<void()> connect_success_notify_ = nullptr;
    std::atomic<bool> is_stoped_{ false };

private:
    boost::asio::io_service ios_;
    boost::asio::io_service::work work_;
    boost::asio::ip::tcp::socket socket_;
    std::unique_ptr<std::thread> thread_ = nullptr;
    std::atomic<bool> is_connected_ { false };
    std::mutex conn_mutex_;
    threadsafe_list<std::shared_ptr<std::string>> send_queue_;
    std::atomic<bool> is_resend_{ false };
};

}
}
