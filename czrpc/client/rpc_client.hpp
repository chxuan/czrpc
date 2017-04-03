#pragma once

#include "client_base.hpp"

namespace czrpc
{
namespace client
{
class rpc_client : public client_base
{
public:
    rpc_client(const rpc_client&) = delete;
    rpc_client& operator=(const rpc_client&) = delete;
    rpc_client() : timer_work_(timer_ios_), timer_(timer_ios_) 
    {
        client_type_ = client_type::rpc_client;
    }

    virtual ~rpc_client()
    {
        stop();
    }

    virtual void run() override final
    {
        client_base::run();
        try_connect();
        start_timer_thread();
    }

    virtual void stop() override final
    {
        stop_timer_thread();
        client_base::stop();
    }
    
    message_ptr call(const std::string& func_name, const message_ptr& message)
    {
        serialize_util::singleton::get()->check_message(message);
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        auto rsp = call_two_way(request_content{ 0, flag, func_name, message->GetDescriptor()->full_name(), 
                                serialize_util::singleton::get()->serialize(message) });
        return serialize_util::singleton::get()->deserialize(rsp.message_name, rsp.body);
    }

    std::string call_raw(const std::string& func_name, const std::string& body)
    {
        try_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        auto rsp = call_two_way(request_content{ 0, flag, func_name, "", body });
        return std::move(rsp.body);
    }

private:
    response_content call_two_way(const request_content& content)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        write(content);
        start_timer();
        auto guard = make_guard([this]{ stop_timer(); });
        return std::move(read());
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

private:
    boost::asio::io_service timer_ios_;
    boost::asio::io_service::work timer_work_;
    std::unique_ptr<std::thread> timer_thread_;
    atimer<> timer_;

    std::mutex mutex_;
};

}
}

