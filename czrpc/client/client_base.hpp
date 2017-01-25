#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <google/protobuf/message.h>
#include <boost/asio.hpp>
#include "base/header.hpp"
#include "base/atimer.hpp"
#include "base/scope_guard.hpp"
#include "base/logger.hpp"
#include "base/serialize_util.hpp"
#include "base/table/threadsafe_list.hpp"

using namespace czrpc::base;
using namespace czrpc::base::table;

namespace czrpc
{
namespace client
{
class client_base
{
public:
    client_base() : work_(ios_), socket_(ios_), 
    timer_work_(timer_ios_), timer_(timer_ios_), is_connected_(false) {}
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

    client_base& timeout(std::size_t timeout_milli)
    {
        timeout_milli_ = timeout_milli;
        return *this;
    }

    virtual void run()
    {
        start_ios_thread();
        start_timer_thread();
    }

    virtual void stop()
    {
        stop_timer_thread();
        stop_ios_thread();
    }

    void call_one_way(const client_flag& flag, const request_content& content)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        write(flag, content);
    }

    std::vector<char> call_two_way(const client_flag& flag, const request_content& content)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        write(flag, content);
        return read();
    }

    void async_call_one_way(const client_flag& flag, const request_content& content)
    {
        async_write(flag, content);
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

protected:
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

private:
    std::string get_buffer(const request_data& data)
    {
        std::string buffer;
        buffer.append(reinterpret_cast<const char*>(&data.header), sizeof(data.header));
        buffer.append(data.content.call_id);
        buffer.append(data.content.protocol);
        buffer.append(data.content.message_name);
        buffer.append(data.content.body);
        return std::move(buffer);
    }

    void connect()
    {
        auto begin_time = std::chrono::high_resolution_clock::now();
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
                auto end_time = std::chrono::high_resolution_clock::now();
                auto elapsed_time = end_time - begin_time;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count() > static_cast<long>(timeout_milli_))
                {
                    throw std::runtime_error(e.what());
                }
            }
        }
    }

    void write(const client_flag& flag, const request_content& content)
    {
        request_header header;
        header.call_id_len = content.call_id.size();
        header.protocol_len = content.protocol.size();
        header.message_name_len = content.message_name.size();
        header.body_len = content.body.size();
        header.flag = flag;

        if (header.call_id_len + header.protocol_len + header.message_name_len + header.body_len > max_buffer_len)
        {
            throw std::runtime_error("Send data is too big");
        }

        std::string buffer = get_buffer(request_data{ header, content });
        write_impl(buffer);
    }

    void async_write(const client_flag& flag, const request_content& content)
    {
        request_header header;
        header.call_id_len = content.call_id.size();
        header.protocol_len = content.protocol.size();
        header.message_name_len = content.message_name.size();
        header.body_len = content.body.size();
        header.flag = flag;

        if (header.call_id_len + header.protocol_len + header.message_name_len + header.body_len > max_buffer_len)
        {
            throw std::runtime_error("Send data is too big");
        }

        std::string buffer = get_buffer(request_data{ header, content });
        async_write_impl(buffer);
    }

    void write_impl(const std::string& buffer)
    {
        boost::system::error_code ec;
        boost::asio::write(socket_, boost::asio::buffer(buffer), ec);
        if (ec)
        {
            is_connected_ = false;
            throw std::runtime_error(ec.message());
        }
    }

    void async_write_impl(const std::string& buffer)
    {
        ios_.post([this, buffer]
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
        boost::asio::async_write(socket_, boost::asio::buffer(send_queue_.front()), 
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
                send_queue_.clear();
                log_warn(ec.message());
            }
        });
    }

    std::vector<char> read()
    {
        start_timer();
        auto guard = make_guard([this]{ stop_timer(); });
        read_head();
        check_head();
        return read_content();
    }

    void read_head()
    {
        boost::system::error_code ec;
        boost::asio::read(socket_, boost::asio::buffer(res_head_buf_), ec);
        if (ec)
        {
            is_connected_ = false;
            throw std::runtime_error(ec.message());
        }
    }

    void check_head()
    {
        memcpy(&res_head_, res_head_buf_, sizeof(res_head_buf_));
        if (res_head_.call_id_len + res_head_.message_name_len + res_head_.body_len > max_buffer_len)
        {
            throw std::runtime_error("Body len is too big");
        }
    }

    std::vector<char> read_content()
    {
        content_.clear();
        content_.resize(res_head_.call_id_len + res_head_.message_name_len + res_head_.body_len);
        boost::system::error_code ec;
        boost::asio::read(socket_, boost::asio::buffer(content_), ec); 
        if (ec)
        {
            is_connected_ = false;
            throw std::runtime_error(ec.message());
        }
        return content_;
    }

    void start_timer()
    {
        if (timeout_milli_ != 0)
        {
            timer_.start(timeout_milli_);
        }
    }

    void stop_timer()
    {
        if (timeout_milli_ != 0)
        {
            timer_.stop();
        }
    }

    void start_ios_thread()
    {
        thread_ = std::make_unique<std::thread>([this]{ ios_.run(); });
    }

    void start_timer_thread()
    {
        if (timeout_milli_ != 0)
        {
            timer_thread_ = std::make_unique<std::thread>([this]{ timer_ios_.run(); });
            timer_.bind([this]{ disconnect(); });
            timer_.set_single_shot(true);
        }
    }

    void stop_timer_thread()
    {
        timer_ios_.stop();
        if (timer_thread_ != nullptr)
        {
            if (timer_thread_->joinable())
            {
                timer_thread_->join();
            }
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
    response_header res_head_;

private:
    boost::asio::io_service ios_;
    boost::asio::io_service::work work_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::resolver::iterator endpoint_iter_;
    std::unique_ptr<std::thread> thread_;
    char res_head_buf_[response_header_len];
    std::vector<char> content_;

    boost::asio::io_service timer_ios_;
    boost::asio::io_service::work timer_work_;
    std::unique_ptr<std::thread> timer_thread_;
    atimer<> timer_;

    std::size_t timeout_milli_ = 0;
    std::atomic<bool> is_connected_ ;
    std::mutex mutex_;
    std::mutex conn_mutex_;

    threadsafe_list<std::string> send_queue_;
};

}
}
