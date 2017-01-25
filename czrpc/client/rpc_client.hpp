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
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = func_name;
        content.message_name = message->GetDescriptor()->full_name();
        content.body = serialize_util::singleton::get()->serialize(message);
        auto ret = call_two_way(flag, content);
        std::string message_name = std::string(&ret[0], res_head_.message_name_len);
        std::string body = std::string(&ret[res_head_.message_name_len], res_head_.body_len);
        return serialize_util::singleton::get()->deserialize(message_name, body);
    }

#if 0
    template<typename ReturnType>
        typename std::enable_if<std::is_same<ReturnType, one_way>::value>::type 
        call_raw(const std::string& protocol, const std::string& body)
        {
            try_connect();
            client_flag flag{ serialize_mode::non_serialize, client_type_ };
            request_content content;
            content.protocol = protocol;
            content.body = body;
            call_one_way(flag, content);
        }

    template<typename ReturnType>
        typename std::enable_if<std::is_same<ReturnType, two_way>::value, std::string>::type 
        call_raw(const std::string& protocol, const std::string& body)
        {
            try_connect();
            client_flag flag{ serialize_mode::non_serialize, client_type_ };
            request_content content;
            content.protocol = protocol;
            content.body = body;
            auto ret = call_two_way(flag, content);
            return std::string(&ret[0], ret.size());
        }
#endif
};

}
}

