#pragma once

#include <unordered_map>
#include <mutex>
#include "base/common_util.hpp"
#include "protocol.hpp"
#include "client_base.hpp"

namespace czrpc
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

    template<typename Protocol, typename... Args>
    typename std::enable_if<std::is_void<typename Protocol::return_type>::value, typename Protocol::return_type>::type
    call(const Protocol& protocol, Args&&... args)
    {
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = protocol.name();
        content.body = serialize(std::forward<Args>(args)...);
        call_one_way(flag, content);
    }

    template<typename Protocol, typename... Args>
    typename std::enable_if<!std::is_void<typename Protocol::return_type>::value, typename Protocol::return_type>::type
    call(const Protocol& protocol, Args&&... args)
    {
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        request_content content;
        content.protocol = protocol.name();
        content.body = serialize(std::forward<Args>(args)...);
        auto ret = call_two_way(flag, content);
        return protocol.deserialize(std::string(&ret[0], ret.size()));
    }

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
};

}

#endif
