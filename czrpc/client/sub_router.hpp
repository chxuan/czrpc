#pragma once

#include <iostream>
#include <unordered_map>
#include <map>
#include <tuple>
#include <type_traits>
#include "base/header.hpp"
#include "base/function_traits.hpp"
#include "base/logger.hpp"
#include "base/singleton.hpp"
#include "base/serialize_util.hpp"
#include "base/thread_pool.hpp"

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

    void operator()(const push_content& content)
    {
        try
        {
            func_(serialize_util::singleton::get()->deserialize(content.message_name, content.body));
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
    using function_t = std::function<void(const std::string& body)>;
    sub_invoker_function_raw() = default;
    sub_invoker_function_raw(const function_t& func) : func_(func) {}

    void operator()(const push_content& content)
    {
        try
        {
            func_(content.body);
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
    }

private:
    function_t func_ = nullptr;
};

class sub_router
{
    DEFINE_SINGLETON(sub_router);
public:
    sub_router()
    {
        static const std::size_t thread_num = 1;
        threadpool_.init_thread_num(thread_num);
    }

    template<typename Function>
    void bind(const std::string& protocol, const Function& func)
    {
        bind_non_member_func(protocol, func);
    }

    template<typename Function, typename Self>
    void bind(const std::string& protocol, const Function& func, Self* self)
    {
        bind_member_func(protocol, func, self); 
    }

    void unbind(const std::string& protocol)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        invoker_map_.erase(protocol);
    }

    bool is_bind(const std::string& protocol)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        auto iter = invoker_map_.find(protocol);
        if (iter != invoker_map_.end())
        {
            return true;
        }
        return false;
    }

    template<typename Function>
    void bind_raw(const std::string& protocol, const Function& func)
    {
        bind_non_member_func_raw(protocol, func);
    }

    template<typename Function, typename Self>
    void bind_raw(const std::string& protocol, const Function& func, Self* self)
    {
        bind_member_func_raw(protocol, func, self); 
    }

    void unbind_raw(const std::string& protocol)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        invoker_raw_map_.erase(protocol);
    }

    bool is_bind_raw(const std::string& protocol)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        auto iter = invoker_raw_map_.find(protocol);
        if (iter != invoker_raw_map_.end())
        {
            return true;
        }
        return false;
    }

    bool route(serialize_mode mode, const push_content& content)
    {
        if (mode == serialize_mode::serialize)
        {
            std::lock_guard<std::mutex> lock(map_mutex_);
            auto iter = invoker_map_.find(content.protocol);
            if (iter == invoker_map_.end())
            {
                return false;
            }
            /* iter->second(content); */
            threadpool_.add_task(iter->second, content);
        }
        else if (mode == serialize_mode::non_serialize)
        {
            std::lock_guard<std::mutex> lock(raw_map_mutex_);
            auto iter = invoker_raw_map_.find(content.protocol);
            if (iter == invoker_raw_map_.end())
            {
                return false;
            }
            /* iter->second(content); */
            threadpool_.add_task(iter->second, content);
        }
        return true;
    }

    std::vector<std::string> get_all_topic()
    {
        std::vector<std::string> topic_vec;

        std::unique_lock<std::mutex> lock(map_mutex_);
        for (auto& invoker : invoker_map_)
        {
            topic_vec.emplace_back(invoker.first);
        }
        lock.unlock();

        std::unique_lock<std::mutex> raw_lock(raw_map_mutex_);
        for (auto& invoker : invoker_raw_map_)
        {
            topic_vec.emplace_back(invoker.first);
        }
        raw_lock.unlock();

        return std::move(topic_vec);
    }

private:
    template<typename Function>
    static typename std::enable_if<std::is_void<typename std::result_of<Function(const message_ptr&)>::type>::value>::type
    call(const Function& func, const message_ptr& in_message)
    {
        func(in_message);
    }

    template<typename Function, typename Self>
    static typename std::enable_if<std::is_void<typename std::result_of<Function(Self, const message_ptr&)>::type>::value>::type
    call_member(const Function& func, Self* self, const message_ptr& in_message)
    {
        (*self.*func)(in_message);
    }

    template<typename Function>
    static typename std::enable_if<std::is_void<typename std::result_of<Function(const std::string&)>::type>::value>::type
    call_raw(const Function& func, const std::string& body)
    {
        func(body);
    }

    template<typename Function, typename Self>
    static typename std::enable_if<std::is_void<typename std::result_of<Function(Self, const std::string&)>::type>::value>::type
    call_member_raw(const Function& func, Self* self, const std::string& body)
    {
        (*self.*func)(body);
    }

private:
    template<typename Function>
    class invoker
    {
    public:
        static void apply(const Function& func, const message_ptr& in_message)
        {
            try
            {
                call(func, in_message);
            }
            catch (std::exception& e)
            {
                log_warn(e.what());
            }
        }

        template<typename Self>
        static void apply_member(const Function& func, Self* self, const message_ptr& in_message)
        {
            try
            {
                call_member(func, self, in_message);
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
        static void apply(const Function& func, const std::string& body)
        {
            try
            {
                call_raw(func, body);
            }
            catch (std::exception& e)
            {
                log_warn(e.what());
            }
        }

        template<typename Self>
        static void apply_member(const Function& func, Self* self, const std::string& body)
        {
            try
            {
                call_member_raw(func, self, body);
            }
            catch (std::exception& e)
            {
                log_warn(e.what());
            }
        }
    }; 

private:
    template<typename Function>
    void bind_non_member_func(const std::string& protocol, const Function& func)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        invoker_map_.emplace(protocol, sub_invoker_function{ std::bind(&invoker<Function>::apply, func, std::placeholders::_1) });
    }

    template<typename Function, typename Self>
    void bind_member_func(const std::string& protocol, const Function& func, Self* self)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        invoker_map_.emplace(protocol, sub_invoker_function{ std::bind(&invoker<Function>::template apply_member<Self>, func, self, std::placeholders::_1) });
    }

    template<typename Function>
    void bind_non_member_func_raw(const std::string& protocol, const Function& func)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        invoker_raw_map_.emplace(protocol, sub_invoker_function_raw{ std::bind(&invoker_raw<Function>::apply, func, std::placeholders::_1) });
    }

    template<typename Function, typename Self>
    void bind_member_func_raw(const std::string& protocol, const Function& func, Self* self)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        invoker_raw_map_.emplace(protocol, sub_invoker_function_raw{ std::bind(&invoker_raw<Function>::template apply_member<Self>, func, self, std::placeholders::_1) });
    }

private:
    std::unordered_map<std::string, sub_invoker_function> invoker_map_;
    std::unordered_map<std::string, sub_invoker_function_raw> invoker_raw_map_;
    std::mutex map_mutex_;
    std::mutex raw_map_mutex_;
    thread_pool threadpool_;
};

}
}

