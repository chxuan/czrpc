#pragma once

#include "client_base.hpp"

namespace czrpc
{
namespace client
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

    void publish(const std::string& topic_name, const message_ptr& message)
    {
        serialize_util::singleton::get()->check_message(message);
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.message_name = message->GetDescriptor()->full_name();
        content.body = serialize_util::singleton::get()->serialize(message);
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

    void async_publish(const std::string& topic_name, const message_ptr& message)
    {
        serialize_util::singleton::get()->check_message(message);
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = topic_name;
        content.message_name = message->GetDescriptor()->full_name();
        content.body = serialize_util::singleton::get()->serialize(message);
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
}

