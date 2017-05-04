#pragma once

#include "client_base.hpp"
#include "rpc_task.hpp"

namespace czrpc
{
namespace client
{
class async_rpc_client : public client_base
{
public:
    async_rpc_client(const async_rpc_client&) = delete;
    async_rpc_client& operator=(const async_rpc_client&) = delete;
    async_rpc_client() : call_id_(0)
    {
        client_type_ = client_type::async_rpc_client;
    }

    virtual ~async_rpc_client()
    {
        stop();
    }

    virtual void run() override final
    {
        static const std::size_t thread_num = 1;
        threadpool_.init_thread_num(thread_num);
        client_base::run();
        start_timer();
        sync_connect();
    }

    virtual void stop() override final
    {
        timer_.destroy();
        client_base::stop();
        threadpool_.stop();
    }

    auto async_call(const std::string& func_name, const message_ptr& message)
    {
        serialize_util::singleton::get()->check_message(message);
        client_flag flag{ serialize_mode::serialize, client_type_ };
        return rpc_task<async_rpc_client>{ request_content{ ++call_id_, flag, func_name, 
                                           message->GetDescriptor()->full_name(), 
                                           serialize_util::singleton::get()->serialize(message) }, this };
    }

    auto async_call_raw(const std::string& func_name, const std::string& body)
    {
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        return rpc_task<async_rpc_client>{ request_content{ ++call_id_, flag, func_name, "", body }, this };
    }

    void add_bind_func(unsigned int call_id, const task_t& task)
    {
        auto begin_time = std::chrono::high_resolution_clock::now();
        task_with_timepoint task_time{ task, begin_time };
        task_map_.emplace(call_id, task_time);
    }

private:
    void async_read_head()
    {
        boost::asio::async_read(get_socket(), boost::asio::buffer(rsp_head_buf_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            if (is_stoped_)
            {
                return;
            }

            if (!get_socket().is_open())
            {
                log_warn() << "Socket is not open";
                handle_error();
                return;
            }
            if (ec)
            {
                log_warn() << ec.message();
                handle_error();
                return;
            }

            if (check_head())
            {
                async_read_content();
            }
            else
            {
                log_warn() << "Content len is too big";
                async_read_head();
            }
        });
    }

    void async_read_content()
    {
        rsp_content_.clear();
        rsp_content_.resize(sizeof(unsigned int) + sizeof(rpc_error_code) + rsp_head_.message_name_len + rsp_head_.body_len);
        boost::asio::async_read(get_socket(), boost::asio::buffer(rsp_content_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            if (is_stoped_)
            {
                return;
            }
            async_read_head();

            if (!get_socket().is_open())
            {
                log_warn() << "Socket is not open";
                handle_error();
                return;
            }
            if (ec)
            {
                log_warn() << ec.message();
                handle_error();
                return;
            }

            route(make_response_content());
        });
    }

    void route(const response_content& content)
    {
        task_with_timepoint task_time;
        if (task_map_.find(content.call_id, task_time))
        {
            task_map_.erase(content.call_id);
            threadpool_.add_task(task_time.task, content);
        }
        else
        {
            log_warn() << "Route failed, call id: " << content.call_id << ", message name: " << content.message_name;
        }
    }

    void check_request_timeout()
    {
        auto current_time = std::chrono::high_resolution_clock::now();
        task_map_.for_each_erase([&](int, const task_with_timepoint& task_time)
        {
            auto elapsed_time = current_time - task_time.time;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count() >= static_cast<long>(timeout_milli_))
            {
                response_content content;
                content.code = rpc_error_code::request_timeout;
                threadpool_.add_task(task_time.task, content);
                return true;
            }
            return false;
        });
    }

    void start_timer()
    {
        timer_.bind([this]{ check_request_timeout(); });
        timer_.start(check_request_timeout_milli);
    }

    void sync_connect()
    {
        if (try_connect())
        {
            start();
        }
    }

    void reconnect()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        boost::asio::async_connect(get_socket(), endpoint_iter_,
                                   [this](boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator)
        {
            if (!ec)
            {
                start();
            }
            else if (ec != boost::asio::error::already_connected)
            {
                reconnect();
            }
        });
    }

    void start()
    {
        task_map_.clear();
        async_read_head();
        if (connect_success_notify_ != nullptr)
        {
            connect_success_notify_();
        }
    }

    void handle_error()
    {
        disconnect();
        reconnect();
    }

private:
    struct task_with_timepoint
    {
        task_t task;
        std::chrono::time_point<std::chrono::high_resolution_clock> time;
    };
    threadsafe_unordered_map<unsigned int, task_with_timepoint> task_map_;
    atimer<> timer_;
    thread_pool threadpool_;
    std::atomic<unsigned int> call_id_;
};

}
}

