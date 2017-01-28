#pragma once

#include <unordered_map>
#include <mutex>
#include "base/common_util.hpp"
#include "base/table/threadsafe_unordered_map.hpp"
#include "client_base.hpp"

using namespace czrpc::base::table;

namespace czrpc
{
namespace client
{
class async_rpc_client : public client_base
{
public:
    async_rpc_client(const async_rpc_client&) = delete;
    async_rpc_client& operator=(const async_rpc_client&) = delete;
    async_rpc_client() : timer_work_(timer_ios_), timer_(timer_ios_) 
    {
        client_type_ = client_type::async_rpc_client;
    }

    virtual void run() override final
    {
        client_base::run();
        sync_connect();
        start_timer_thread();
    }

    virtual void stop() override final
    {
        stop_timer_thread();
        client_base::stop();
    }

    using task_t = std::function<void(const response_content&)>; 
    class rpc_task
    {
    public:
        rpc_task(const client_flag& flag, const request_content& content, async_rpc_client* client) 
            : flag_(flag), content_(content), client_(client) {}

        void result(const std::function<void(const message_ptr&)>& func)
        {
            task_ = [func, this](const response_content& content)
            {
                try
                {
                    func(serialize_util::singleton::get()->deserialize(content.message_name, content.body));
                }
                catch (std::exception& e)
                {
                    log_warn(e.what());
                }
            };
            client_->async_call_one_way(flag_, content_);
            client_->add_bind_func(content_.call_id, task_);
        }

        void result(const std::function<void(const std::string&)>& func)
        {
            task_ = [func, this](const response_content& content)
            {
                try
                {
                    func(content.body);
                }
                catch (std::exception& e)
                {
                    log_warn(e.what());
                }
            };
            client_->async_call_one_way(flag_, content_);
            client_->add_bind_func(content_.call_id, task_);
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
        request_content content;
        content.call_id = gen_uuid();
        content.protocol = func_name;
        content.message_name = message->GetDescriptor()->full_name();
        content.body = serialize_util::singleton::get()->serialize(message);

        client_flag flag{ serialize_mode::serialize, client_type_ };
        return rpc_task{ flag, content, this };
    }

    auto async_call_raw(const std::string& func_name, const std::string& body)
    {
        sync_connect();
        request_content content;
        content.call_id = gen_uuid();
        content.protocol = func_name;
        content.body = body;

        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        return rpc_task{ flag, content, this };
    }

private:
    void async_read_head()
    {
        boost::asio::async_read(get_socket(), boost::asio::buffer(res_head_buf_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            if (!get_socket().is_open())
            {
                log_warn("Socket is not open");
                return;
            }

            if (ec)
            {
                log_warn(ec.message());
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
        if (res_head_.call_id_len + res_head_.message_name_len + res_head_.body_len > max_buffer_len)
        {
            log_warn("Content len is too big");
            return false;
        }
        return true;
    }

    void async_read_content()
    {
        content_.clear();
        content_.resize(res_head_.call_id_len + res_head_.message_name_len + res_head_.body_len);
        boost::asio::async_read(get_socket(), boost::asio::buffer(content_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            async_read_head();

            if (!get_socket().is_open())
            {
                log_warn("Socket is not open");
                return;
            }

            if (ec)
            {
                log_warn(ec.message());
                return;
            }

            response_content content;
            content.call_id.assign(&content_[0], res_head_.call_id_len);
            content.message_name.assign(&content_[res_head_.call_id_len], res_head_.message_name_len);
            content.body.assign(&content_[res_head_.call_id_len + res_head_.message_name_len], res_head_.body_len);
            if (res_head_.error_code == rpc_error_code::ok)
            {
                route(content);
            }
            else
            {
                log_warn(get_rpc_error_string(res_head_.error_code));
                task_map_.erase(content.call_id);
            }
        });
    }

    void add_bind_func(const std::string& call_id, const task_t& task)
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
            task_time.task(content);
            task_map_.erase(content.call_id);
            std::cout << "map size: " << task_map_.size() << std::endl;
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
        std::cout << "#################### time out: " << timeout_milli_ << std::endl;
        auto current_time = std::chrono::high_resolution_clock::now();
        task_map_.for_each_erase([&](const std::string&, const task_with_timepoint& task_time)
        {
            auto elapsed_time = current_time - task_time.time;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count() > static_cast<long>(timeout_milli_))
            {
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
            timer_.start(timeout_milli_);
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
    threadsafe_unordered_map<std::string, task_with_timepoint> task_map_;
};

}
}

