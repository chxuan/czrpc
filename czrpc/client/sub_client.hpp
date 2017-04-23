#pragma once

#include "base/thread_pool.hpp"
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
    sub_client() 
    {
        client_type_ = client_type::sub_client;
    }

    virtual ~sub_client()
    {
        stop();
    }

    virtual void run() override final
    {
        static const std::size_t thread_num = 1;
        threadpool_.init_thread_num(thread_num);
        client_base::run();
        sync_connect();
    }

    virtual void stop() override final
    {
        client_base::stop();
        threadpool_.stop();
    }

    template<typename Function>
    void subscribe(const std::string& topic_name, const Function& func)
    {
        client_flag flag{ serialize_mode::serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
        sub_router::singleton::get()->bind(topic_name, func);
    }

    template<typename Function, typename Self>
    void subscribe(const std::string& topic_name, const Function& func, Self* self)
    {
        client_flag flag{ serialize_mode::serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
        sub_router::singleton::get()->bind(topic_name, func, self);
    }

    template<typename Function>
    void subscribe_raw(const std::string& topic_name, const Function& func)
    {
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
        sub_router::singleton::get()->bind_raw(topic_name, func);
    }

    template<typename Function, typename Self>
    void subscribe_raw(const std::string& topic_name, const Function& func, Self* self)
    {
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
        sub_router::singleton::get()->bind_raw(topic_name, func, self);
    }

    void cancel_subscribe(const std::string& topic_name)
    {
        client_flag flag{ serialize_mode::serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", cancel_subscribe_topic_flag });
        sub_router::singleton::get()->unbind(topic_name);
    }

    void cancel_subscribe_raw(const std::string& topic_name)
    {
        client_flag flag{ serialize_mode::serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", cancel_subscribe_topic_flag });
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
                log_warn() << "Socket is not open";
                reconnect();
                return;
            }

            if (ec)
            {
                log_warn() << ec.message();
                reconnect();
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

    bool check_head()
    {
        memcpy(&push_head_, push_head_buf_, sizeof(push_head_buf_));
        if (push_head_.protocol_len + push_head_.message_name_len + push_head_.body_len > max_buffer_len)
        {
            return false;
        }
        return true;
    }

    void async_read_content()
    {
        content_.clear();
        content_.resize(sizeof(serialize_mode) + push_head_.protocol_len + push_head_.message_name_len + push_head_.body_len);
        boost::asio::async_read(get_socket(), boost::asio::buffer(content_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            async_read_head();

            if (!get_socket().is_open())
            {
                log_warn() << "Socket is not open";
                reconnect();
                return;
            }

            if (ec)
            {
                log_warn() << ec.message();
                reconnect();
                return;
            }

            threadpool_.add_task(&sub_client::router_thread, this, make_push_content());
        });
    }

    push_content make_push_content()
    {
        push_content content;
        memcpy(&content.mode, &content_[0], sizeof(content.mode));
        content.protocol.assign(&content_[sizeof(content.mode)], push_head_.protocol_len);
        content.message_name.assign(&content_[sizeof(content.mode) + push_head_.protocol_len], push_head_.message_name_len);
        content.body.assign(&content_[sizeof(content.mode) + push_head_.protocol_len + push_head_.message_name_len], push_head_.body_len);
        return std::move(content);
    }

    void retry_subscribe()
    {
        try
        {
            for (auto& topic_name : sub_router::singleton::get()->get_all_topic())
            {
                client_flag flag{ serialize_mode::serialize, client_type_ };
                async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
            }
        }
        catch (std::exception& e)
        {
            log_warn() << e.what();
        }
    }

    void router_thread(const push_content& content)
    {
        bool ok = false;
        if (content.mode == serialize_mode::serialize)
        {
            message_ptr req = serialize_util::singleton::get()->deserialize(content.message_name, content.body);
            ok = sub_router::singleton::get()->route(content.protocol, req);
        }
        else if (content.mode == serialize_mode::non_serialize)
        {
            ok = sub_router::singleton::get()->route_raw(content.protocol, content.body);
        }
        if (!ok)
        {
            log_warn() << "Route failed";
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

    void reconnect()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        boost::asio::async_connect(get_socket(), endpoint_iter_,
                                   [this](boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator)
        {
            if (!ec)
            {
                async_read_head();
                retry_subscribe();
            }
            else if (ec != boost::asio::error::already_connected)
            {
                reconnect();
            }
        });
    }

private:
    char push_head_buf_[push_header_len];
    push_header push_head_;
    std::vector<char> content_;
    thread_pool threadpool_;
};

}
}

