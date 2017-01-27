#pragma once

#include "client_base.hpp"
#include "sub_router.hpp"

namespace czrpc
{
namespace client
{
class sub_client : public client_base
{
public:
    sub_client(const sub_client&) = delete;
    sub_client& operator=(const sub_client&) = delete;
    sub_client() : heartbeats_work_(heartbeats_ios_), heartbeats_timer_(heartbeats_ios_)
    {
        client_type_ = client_type::sub_client;
    }

    virtual ~sub_client()
    {
        stop();
    }

    virtual void run() override final
    {
        client_base::run();
        sync_connect();
        start_heartbeats_thread();
    }

    virtual void stop() override final
    {
        stop_heartbeats_thread();
        client_base::stop();
    }

    template<typename Function>
    void subscribe(const std::string& topic_name, const Function& func)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = subscribe_topic_flag;
        call_one_way(flag, content);
        sub_router::singleton::get()->bind(topic_name, func);
    }

    template<typename Function, typename Self>
    void subscribe(const std::string& topic_name, const Function& func, Self* self)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = subscribe_topic_flag;
        call_one_way(flag, content);
        sub_router::singleton::get()->bind(topic_name, func, self);
    }

    template<typename Function>
    void async_subscribe(const std::string& topic_name, const Function& func)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = subscribe_topic_flag;
        async_call_one_way(flag, content);
        sub_router::singleton::get()->bind(topic_name, func);
    }

    template<typename Function, typename Self>
    void async_subscribe(const std::string& topic_name, const Function& func, Self* self)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = subscribe_topic_flag;
        async_call_one_way(flag, content);
        sub_router::singleton::get()->bind(topic_name, func, self);
    }

    template<typename Function>
    void subscribe_raw(const std::string& topic_name, const Function& func)
    {
        sync_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = subscribe_topic_flag;
        call_one_way(flag, content);
        sub_router::singleton::get()->bind_raw(topic_name, func);
    }

    template<typename Function, typename Self>
    void subscribe_raw(const std::string& topic_name, const Function& func, Self* self)
    {
        sync_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = subscribe_topic_flag;
        call_one_way(flag, content);
        sub_router::singleton::get()->bind_raw(topic_name, func, self);
    }

    template<typename Function>
    void async_subscribe_raw(const std::string& topic_name, const Function& func)
    {
        sync_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = subscribe_topic_flag;
        async_call_one_way(flag, content);
        sub_router::singleton::get()->bind_raw(topic_name, func);
    }

    template<typename Function, typename Self>
    void async_subscribe_raw(const std::string& topic_name, const Function& func, Self* self)
    {
        sync_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = subscribe_topic_flag;
        async_call_one_way(flag, content);
        sub_router::singleton::get()->bind_raw(topic_name, func, self);
    }

    void cancel_subscribe(const std::string& topic_name)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = cancel_subscribe_topic_flag;
        call_one_way(flag, content);
        sub_router::singleton::get()->unbind(topic_name);
    }

    void cancel_subscribe_raw(const std::string& topic_name)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = cancel_subscribe_topic_flag;
        call_one_way(flag, content);
        sub_router::singleton::get()->unbind_raw(topic_name);
    }

    void async_cancel_subscribe(const std::string& topic_name)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = cancel_subscribe_topic_flag;
        async_call_one_way(flag, content);
        sub_router::singleton::get()->unbind(topic_name);
    }

    void async_cancel_subscribe_raw(const std::string& topic_name)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = cancel_subscribe_topic_flag;
        async_call_one_way(flag, content);
        sub_router::singleton::get()->unbind_raw(topic_name);
    }

    bool is_subscribe(const std::string& topic_name)
    {
        return sub_router::singleton::get()->is_bind(topic_name);
    }

    bool is_subscribe_raw(const std::string& topic_name)
    {
        return sub_router::singleton::get()->is_bind_raw(topic_name);
    }

private:
    void async_read_head()
    {
        boost::asio::async_read(get_socket(), boost::asio::buffer(push_head_buf_), 
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
        memcpy(&push_head_, push_head_buf_, sizeof(push_head_buf_));
        unsigned int len = push_head_.protocol_len + push_head_.message_name_len + push_head_.body_len;
        return (len > 0 && len < max_buffer_len) ? true : false;
    }

    void async_read_content()
    {
        content_.clear();
        content_.resize(push_head_.protocol_len + push_head_.message_name_len + push_head_.body_len);
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

            push_content content;
            content.protocol.assign(&content_[0], push_head_.protocol_len);
            content.message_name.assign(&content_[push_head_.protocol_len], push_head_.message_name_len);
            content.body.assign(&content_[push_head_.protocol_len + push_head_.message_name_len], push_head_.body_len);
            bool ok = sub_router::singleton::get()->route(push_head_.mode, content);
            if (!ok)
            {
                log_warn("Router failed");
                return;
            }
        });
    }

    void heartbeats_timer()
    {
        std::cout << "###################################### heartbeats_timer" << std::endl;
        try
        {
            sync_connect();
            client_flag flag{ serialize_mode::serialize, client_type_ };
            request_content content;
            content.protocol = heartbeats_flag;
            content.body = heartbeats_flag;
            async_call_one_way(flag, content);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }

    void retry_subscribe()
    {
        try
        {
            for (auto& topic_name : sub_router::singleton::get()->get_all_topic())
            {
                client_flag flag{ serialize_mode::serialize, client_type_ };
                request_content content;
                content.protocol = topic_name;
                content.body = subscribe_topic_flag;
                async_call_one_way(flag, content);
            }
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }

    void start_heartbeats_thread()
    {
        heatbeats_thread_ = std::make_unique<std::thread>([this]{ heartbeats_ios_.run(); });
        heartbeats_timer_.bind([this]{ heartbeats_timer(); });
        heartbeats_timer_.start(heartbeats_milli);
    }

    void stop_heartbeats_thread()
    {
        heartbeats_ios_.stop();
        if (heatbeats_thread_ != nullptr)
        {
            if (heatbeats_thread_->joinable())
            {
                heatbeats_thread_->join();
            }
        }
    }

    void sync_connect()
    {
        if (try_connect())
        {
            async_read_head();
            retry_subscribe();
        }
    }

private:
    char push_head_buf_[push_header_len];
    push_header push_head_;
    std::vector<char> content_;

    boost::asio::io_service heartbeats_ios_;
    boost::asio::io_service::work heartbeats_work_;
    std::unique_ptr<std::thread> heatbeats_thread_;
    atimer<> heartbeats_timer_;
};

}
}

