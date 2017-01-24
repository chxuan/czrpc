#pragma once

#include "client_base.hpp"

namespace czrpc
{

class pub_client : public client_base
{
public:
    pub_client(const pub_client&) = delete;
    pub_client& operator=(const pub_client&) = delete;
    pub_client() 
    {
        client_type_ = client_type::pub_client;
    }

    virtual void run() override final
    {
        client_base::run();
        try_connect();
    }

    template<typename... Args>
    void publish(const std::string& topic_name, Args&&... args)
    {
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = serialize(std::forward<Args>(args)...);
        call_one_way(flag, content);
    }

    void publish_raw(const std::string& topic_name, const std::string& body)
    {
        try_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = body;
        call_one_way(flag, content);
    }

    template<typename... Args>
    void async_publish(const std::string& topic_name, Args&&... args)
    {
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = serialize(std::forward<Args>(args)...);
        async_call_one_way(flag, content);
    }

    void async_publish_raw(const std::string& topic_name, const std::string& body)
    {
        try_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.body = body;
        async_call_one_way(flag, content);
    }
};

}

#endif
