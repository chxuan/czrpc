#pragma once

#include <unordered_map>
#include <tuple>
#include <type_traits>
#include "base/header.hpp"
#include "base/function_traits.hpp"
#include "base/logger.hpp"
#include "base/serialize_util.hpp"

namespace czrpc
{
namespace server
{
template<std::size_t N, std::size_t M>
struct is_number_equal : std::integral_constant<bool, N == M> {};

class invoker_function
{
public:
    using function_t = std::function<void(const message_ptr&, const std::string&, message_ptr&)>;
    invoker_function() = default;
    invoker_function(const function_t& func) : func_(func) {}

    void operator()(const std::string& session_id, const message_ptr& req, message_ptr& rsp)
    {
        try
        {
            func_(req, session_id, rsp);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }

private:
    function_t func_ = nullptr;
};

class invoker_function_raw
{
public:
    using function_t = std::function<void(const std::string&, const std::string&, std::string&)>;
    invoker_function_raw() = default;
    invoker_function_raw(const function_t& func) : func_(func) {}

    void operator()(const std::string& session_id, const std::string& req, std::string& rsp)
    {
        try
        {
            func_(req, session_id, rsp);
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
static typename std::enable_if<is_number_equal<1, function_traits<Function>::arity>::value>::type 
call(const Function& func, const message_ptr& req, const std::string&, message_ptr& rsp)
{
    rsp = func(req);
}

template<typename Function>
static typename std::enable_if<is_number_equal<2, function_traits<Function>::arity>::value>::type 
call(const Function& func, const message_ptr& req, const std::string& session_id, message_ptr& rsp)
{
    rsp = func(req, session_id);
}

template<typename Function, typename Self>
static typename std::enable_if<is_number_equal<1, function_traits<Function>::arity>::value>::type 
call_member(const Function& func, Self* self, const message_ptr& req, const std::string&, message_ptr& rsp)
{
    rsp = (*self.*func)(req);
}

template<typename Function, typename Self>
static typename std::enable_if<is_number_equal<2, function_traits<Function>::arity>::value>::type 
call_member(const Function& func, Self* self, const message_ptr& req, const std::string& session_id, message_ptr& rsp)
{
    rsp = (*self.*func)(req, session_id);
}

template<typename Function>
static typename std::enable_if<is_number_equal<1, function_traits<Function>::arity>::value>::type 
call_raw(const Function& func, const std::string& req, const std::string&, std::string& rsp)
{
    rsp = func(req);
}

template<typename Function>
static typename std::enable_if<is_number_equal<2, function_traits<Function>::arity>::value>::type 
call_raw(const Function& func, const std::string& req, const std::string& session_id, std::string& rsp)
{
    rsp = func(req, session_id);
}

template<typename Function, typename Self>
static typename std::enable_if<is_number_equal<1, function_traits<Function>::arity>::value>::type 
call_member_raw(const Function& func, Self* self, const std::string& req, const std::string&, std::string& rsp)
{
    rsp = (*self.*func)(req);
}

template<typename Function, typename Self>
static typename std::enable_if<is_number_equal<2, function_traits<Function>::arity>::value>::type 
call_member_raw(const Function& func, Self* self, const std::string& req, const std::string& session_id, std::string& rsp)
{
    rsp = (*self.*func)(req, session_id);
}

template<typename Function>
class invoker
{
public:
    static void apply(const Function& func, const message_ptr& req, const std::string& session_id, message_ptr& rsp)
    {
        call(func, req, session_id, rsp);
    }

    template<typename Self>
    static void apply_member(const Function& func, Self* self, const message_ptr& req, const std::string& session_id, message_ptr& rsp)
    {
        call_member(func, self, req, session_id, rsp);
    }
}; 

template<typename Function>
class invoker_raw
{
public:
    static void apply(const Function& func, const std::string& req, const std::string& session_id, std::string& rsp)
    {
        call_raw(func, req, session_id, rsp);
    }

    template<typename Self>
    static void apply_member(const Function& func, Self* self, const std::string& req, const std::string& session_id, std::string& rsp)
    {
        call_member_raw(func, self, req, session_id, rsp);
    }
}; 

}
}
