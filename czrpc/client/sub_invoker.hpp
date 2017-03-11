#pragma once

#include <tuple>
#include <type_traits>
#include "base/header.hpp"
#include "base/serialize_util.hpp"
#include "base/function_traits.hpp"
#include "base/logger.hpp"

using namespace czrpc::base;
namespace czrpc
{
namespace client
{

class sub_invoker_function
{
public:
    using function_t = std::function<void(const message_ptr&)>;
    sub_invoker_function() = default;
    sub_invoker_function(const function_t& func) : func_(func) {}

    void operator()(const message_ptr& req)
    {
        try
        {
            func_(req);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }

private:
    function_t func_ = nullptr;
};

class sub_invoker_function_raw
{
public:
    using function_t = std::function<void(const std::string& req)>;
    sub_invoker_function_raw() = default;
    sub_invoker_function_raw(const function_t& func) : func_(func) {}

    void operator()(const std::string& req)
    {
        try
        {
            func_(req);
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
static typename std::enable_if<std::is_void<typename std::result_of<Function(const message_ptr&)>::type>::value>::type
call(const Function& func, const message_ptr& req)
{
    func(req);
}

template<typename Function, typename Self>
static typename std::enable_if<std::is_void<typename std::result_of<Function(Self, const message_ptr&)>::type>::value>::type
call_member(const Function& func, Self* self, const message_ptr& req)
{
    (*self.*func)(req);
}

template<typename Function>
static typename std::enable_if<std::is_void<typename std::result_of<Function(const std::string&)>::type>::value>::type
call_raw(const Function& func, const std::string& req)
{
    func(req);
}

template<typename Function, typename Self>
static typename std::enable_if<std::is_void<typename std::result_of<Function(Self, const std::string&)>::type>::value>::type
call_member_raw(const Function& func, Self* self, const std::string& req)
{
    (*self.*func)(req);
}

template<typename Function>
class invoker
{
public:
    static void apply(const Function& func, const message_ptr& req)
    {
        try
        {
            call(func, req);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }

    template<typename Self>
    static void apply_member(const Function& func, Self* self, const message_ptr& req)
    {
        try
        {
            call_member(func, self, req);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }
}; 

template<typename Function>
class invoker_raw
{
public:
    static void apply(const Function& func, const std::string& req)
    {
        try
        {
            call_raw(func, req);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }

    template<typename Self>
    static void apply_member(const Function& func, Self* self, const std::string& req)
    {
        try
        {
            call_member_raw(func, self, req);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }
}; 

}
}
