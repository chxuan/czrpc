#pragma once

#include <unordered_map>
#include <mutex>
#include "base/common_util.hpp"
#include "client_base.hpp"

namespace czrpc
{

class async_rpc_client : public client_base
{
public:
    async_rpc_client(const async_rpc_client&) = delete;
    async_rpc_client& operator=(const async_rpc_client&) = delete;
    async_rpc_client() 
    {
        client_type_ = client_type::rpc_client;
    }

    virtual void run() override final
    {
        client_base::run();
        sync_connect();
    }

    using task_t = std::function<void(const std::string&)>; 
    template<typename ReturnType>
    class rpc_task
    {
    public:
        rpc_task(const client_flag& flag, const request_content& content, async_rpc_client* client) 
            : flag_(flag), content_(content), client_(client) {}

        template<typename Function>
        void result(const Function& func)
        {
            task_ = [&func, this](const std::string& body)
            {
                (void)body;
                /* return func(deserialize(std::string(&body[0], body.size()))); */ 
            };
            client_->async_call_one_way(flag_, content_);
            client_->add_bind_func(content_.call_id, task_);
        }

    private:
#if 0
        ReturnType deserialize(const std::string& text) 
        {
            easypack::unpack up(text);
            ReturnType ret;
            up.unpack_args(ret);
            return std::move(ret);
        }
#endif

    private:
        client_flag flag_;
        request_content content_;
        task_t task_;
        async_rpc_client* client_;
    };

#if 0
    template<typename Protocol, typename... Args>
    auto async_call(const Protocol& protocol, Args&&... args)
    {
        sync_connect();
        request_content content;
        content.call_id = gen_uuid();
        content.protocol = protocol.name();
        content.body = serialize(std::forward<Args>(args)...);

        client_flag flag{ serialize_mode::serialize, client_type_ };
        using return_type = typename Protocol::return_type;
        return rpc_task<return_type>{ flag, content, this };
    }
#endif

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
        });
    }

    bool async_check_head()
    {
        memcpy(&res_head_, res_head_buf_, sizeof(res_head_buf_));
        unsigned int len = res_head_.call_id_len + res_head_.body_len;
        return (len > 0 && len < max_buffer_len) ? true : false;
    }

    void async_read_content()
    {
        content_.clear();
        content_.resize(res_head_.call_id_len + res_head_.body_len);
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

            std::string call_id;
            call_id.assign(&content_[0], res_head_.call_id_len);
            std::string body;
            body.assign(&content_[res_head_.call_id_len], res_head_.body_len);
            route(call_id, body);
        });
    }

    void add_bind_func(const std::string& call_id, const task_t& task)
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        task_map_.emplace(call_id, task);
    }

    void route(const std::string& call_id, const std::string& body)
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        auto iter = task_map_.find(call_id);
        if (iter != task_map_.end())
        {
            iter->second(body);
            task_map_.erase(iter);
            std::cout << "map size: " << task_map_.size() << std::endl;
        }
    }

    void sync_connect()
    {
        if (try_connect())
        {
            async_read_head();
        }
    }

private:
    char res_head_buf_[response_header_len];
    response_header res_head_;
    std::vector<char> content_;

    std::unordered_map<std::string, task_t> task_map_;
    std::mutex task_mutex_;
};

}

