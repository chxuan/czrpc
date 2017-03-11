#pragma once

#include <unordered_map>
#include "base/singleton.hpp"
#include "sub_invoke.hpp"

namespace czrpc
{
namespace client
{

class sub_router
{
    DEFINE_SINGLETON(sub_router);
public:
    sub_router() = default;

    template<typename Function>
    void bind(const std::string& protocol, const Function& func)
    {
        check_bind(protocol);
        bind_non_member_func(protocol, func);
    }

    template<typename Function, typename Self>
    void bind(const std::string& protocol, const Function& func, Self* self)
    {
        check_bind(protocol);
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
        check_bind_raw(protocol);
        bind_non_member_func_raw(protocol, func);
    }

    template<typename Function, typename Self>
    void bind_raw(const std::string& protocol, const Function& func, Self* self)
    {
        check_bind_raw(protocol);
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

    bool route_raw(const std::string& protocol, const std::string& req)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        auto iter = invoker_raw_map_.find(protocol);
        if (iter != invoker_raw_map_.end())
        {
            iter->second(req);
            return true;
        }
        return false;
    }

    bool route(const std::string& protocol, const std::string& message_name, const std::string& req)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        auto iter = invoker_map_.find(protocol);
        if (iter != invoker_map_.end())
        {
            iter->second(message_name, req);
            return true;
        }
        return false;
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

    void check_bind(const std::string& protocol)
    {
        if (is_bind(protocol))
        {
            throw std::runtime_error(protocol + " was binded");
        }
    }

    void check_bind_raw(const std::string& protocol)
    {
        if (is_bind_raw(protocol))
        {
            throw std::runtime_error(protocol + " was binded");
        }
    }

private:
    std::unordered_map<std::string, sub_invoker_function> invoker_map_;
    std::unordered_map<std::string, sub_invoker_function_raw> invoker_raw_map_;
    std::mutex map_mutex_;
    std::mutex raw_map_mutex_;
};

}
}

