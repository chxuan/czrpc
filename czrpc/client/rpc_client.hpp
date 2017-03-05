#pragma once

#include <unordered_map>
#include <mutex>
#include "base/common_util.hpp"
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
    rpc_client() 
    {
        client_type_ = client_type::rpc_client;
    }

    virtual void run() override final
    {
        client_base::run();
        try_connect();
    }

    message_ptr call(const std::string& func_name, const message_ptr& message)
    {
        serialize_util::singleton::get()->check_message(message);
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = func_name;
        content.message_name = message->GetDescriptor()->full_name();
        content.body = serialize_util::singleton::get()->serialize(message);
        auto rsp = call_two_way(flag, content);
        return serialize_util::singleton::get()->deserialize(rsp.message_name, rsp.body);
    }

    std::string call_raw(const std::string& func_name, const std::string& body)
    {
        try_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        request_content content;
        content.protocol = func_name;
        content.body = body;
        auto rsp = call_two_way(flag, content);
        return std::move(rsp.body);
    }
};

}
}

