#pragma once

#include <unordered_map>
#include <tuple>
#include <type_traits>
#include "rpc_message.hpp"
#include "base/header.hpp"
#include "base/function_traits.hpp"
#include "base/logger.hpp"

namespace czrpc
{
namespace server
{
class invoker_function
{
public:
    using function_t = std::function<void(const rpc_request_ptr&, const rpc_response_ptr&)>;
    invoker_function() = default;
    invoker_function(const function_t& func) : func_(func) {}

    void operator()(const rpc_request_ptr& req, const rpc_response_ptr& rsp)
    {
        try
        {
            func_(req, rsp);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }

private:
    function_t func_ = nullptr;
};

template<typename Function>
static void call(const Function& func, const rpc_request_ptr& req, const rpc_response_ptr& rsp)
{
    func(req, rsp);
}

template<typename Function, typename Self>
static void call_member(const Function& func, Self* self, const rpc_request_ptr& req, const rpc_response_ptr& rsp)
{
    (*self.*func)(req, rsp);
}

template<typename Function>
class invoker
{
public:
    static void apply(const Function& func, const rpc_request_ptr& req, const rpc_response_ptr& rsp)
    {
        call(func, req, rsp);
    }

    template<typename Self>
    static void apply_member(const Function& func, Self* self, const rpc_request_ptr& req, const rpc_response_ptr& rsp)
    {
        call_member(func, self, req, rsp);
    }
}; 

}
}
