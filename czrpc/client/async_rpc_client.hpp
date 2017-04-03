#pragma once

#include "client_base.hpp"

namespace czrpc
{
namespace client
{
class async_rpc_client : public client_base
{
public:
    async_rpc_client(const async_rpc_client&) = delete;
    async_rpc_client& operator=(const async_rpc_client&) = delete;
    async_rpc_client() : timer_work_(timer_ios_), timer_(timer_ios_), call_id_(0)
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
        sync_connect();
        start_timer_thread();
    }

    virtual void stop() override final
    {
        stop_timer_thread();
        client_base::stop();
        threadpool_.stop();
    }

    using task_t = std::function<void(const response_content&)>; 
    class rpc_task
    {
    public:
        rpc_task(const request_content& content, async_rpc_client* client) 
            : content_(content), client_(client) {}

        void result(const std::function<void(const message_ptr&, const czrpc::base::error_code&)>& func)
        {
            task_ = [func, this](const response_content& content)
            {
                try
                {
                    czrpc::base::error_code ec(content.code);
                    if (ec)
                    {
                        func(nullptr, ec);
                    }
                    else
                    {
                        func(serialize_util::singleton::get()->deserialize(content.message_name, content.body), ec);
                    }
                }
                catch (std::exception& e)
                {
                    std::cout << e.what() << std::endl;
                }
            };
            client_->add_bind_func(content_.call_id, task_);
            client_->async_write(content_);
        }

        void result(const std::function<void(const std::string&, const czrpc::base::error_code&)>& func)
        {
            task_ = [func, this](const response_content& content)
            {
                try
                {
                    czrpc::base::error_code ec(content.code);
                    if (ec)
                    {
                        func("", ec);
                    }
                    else
                    {
                        func(content.body, ec);
                    }
                }
                catch (std::exception& e)
                {
                    std::cout << e.what() << std::endl;
                }
            };
            client_->add_bind_func(content_.call_id, task_);
            client_->async_write(content_);
        }

    private:
        client_flag flag_;
        request_content content_;
        task_t task_;
        async_rpc_client* client_;
    };

    auto async_call(const std::string& func_name, const message_ptr& message)
    {
        serialize_util::singleton::get()->check_message(message);
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        return rpc_task{ request_content{ ++call_id_, flag, func_name, 
                         message->GetDescriptor()->full_name(), 
                         serialize_util::singleton::get()->serialize(message) }, this };
    }

    auto async_call_raw(const std::string& func_name, const std::string& body)
    {
        sync_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        return rpc_task{ request_content{ ++call_id_, flag, func_name, "", body }, this };
    }

private:
    void async_read_head()
    {
        boost::asio::async_read(get_socket(), boost::asio::buffer(res_head_buf_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            if (!get_socket().is_open())
            {
                std::cout << "Socket is not open" << std::endl;
                return;
            }

            if (ec)
            {
                std::cout << ec.message() << std::endl;
                return;
            }

            if (async_check_head())
            {
                async_read_content();
            }
            else
            {
                async_read_head();
            }
        });
    }

    bool async_check_head()
    {
        memcpy(&res_head_, res_head_buf_, sizeof(res_head_buf_));
        if (res_head_.message_name_len + res_head_.body_len > max_buffer_len)
        {
            std::cout << "Content len is too big" << std::endl;
            return false;
        }
        return true;
    }

    void async_read_content()
    {
        content_.clear();
        content_.resize(sizeof(unsigned int) + sizeof(rpc_error_code) + res_head_.message_name_len + res_head_.body_len);
        boost::asio::async_read(get_socket(), boost::asio::buffer(content_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            async_read_head();

            if (!get_socket().is_open())
            {
                std::cout << "Socket is not open" << std::endl;
                return;
            }

            if (ec)
            {
                std::cout << ec.message() << std::endl;
                return;
            }

            route(make_content());
        });
    }

    response_content make_content()
    {
        response_content content;
        memcpy(&content.call_id, &content_[0], sizeof(content.call_id));
        memcpy(&content.code, &content_[sizeof(content.call_id)], sizeof(content.code));
        content.message_name.assign(&content_[sizeof(content.call_id) + sizeof(content.code)], res_head_.message_name_len);
        content.body.assign(&content_[sizeof(content.call_id) + sizeof(content.code) + res_head_.message_name_len], res_head_.body_len);
        return std::move(content);
    }

    void add_bind_func(unsigned int call_id, const task_t& task)
    {
        auto begin_time = std::chrono::high_resolution_clock::now();
        task_with_timepoint task_time{ task, begin_time };
        task_map_.emplace(call_id, task_time);
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
            std::cout << "Route failed, call id: " << content.call_id << ", message name: " << content.message_name << std::endl;
        }
    }

    void sync_connect()
    {
        if (try_connect())
        {
            task_map_.clear();
            async_read_head();
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

    void start_timer_thread()
    {
        if (timeout_milli_ != 0)
        {
            timer_thread_ = std::make_unique<std::thread>([this]{ timer_ios_.run(); });
            timer_.bind([this]{ check_request_timeout(); });
            timer_.start(check_request_timeout_milli);
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

private:
    char res_head_buf_[response_header_len];
    response_header res_head_;
    std::vector<char> content_;

    boost::asio::io_service timer_ios_;
    boost::asio::io_service::work timer_work_;
    std::unique_ptr<std::thread> timer_thread_;
    atimer<> timer_;

    struct task_with_timepoint
    {
        task_t task;
        std::chrono::time_point<std::chrono::high_resolution_clock> time;
    };
    threadsafe_unordered_map<unsigned int, task_with_timepoint> task_map_;
    thread_pool threadpool_;
    std::atomic<unsigned int> call_id_;
};

}
}

