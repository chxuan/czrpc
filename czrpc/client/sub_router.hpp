#pragma once

#include "base/table/threadsafe_unordered_map.hpp"
#include "base/singleton.hpp"
#include "sub_invoker.hpp"

using namespace czrpc::base::table;

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
        route_map_.replace(protocol, sub_invoker_function{ std::bind(&invoker<Function>::apply, func, std::placeholders::_1) });
    }

    template<typename Function, typename Self>
    void bind(const std::string& protocol, const Function& func, Self* self)
    {
        route_map_.replace(protocol, sub_invoker_function{ std::bind(&invoker<Function>::template apply_member<Self>, func, self, std::placeholders::_1) });
    }

    template<typename Function>
    void bind_raw(const std::string& protocol, const Function& func)
    {
        route_raw_map_.replace(protocol, sub_invoker_function_raw{ std::bind(&invoker_raw<Function>::apply, func, std::placeholders::_1) });
    }

    template<typename Function, typename Self>
    void bind_raw(const std::string& protocol, const Function& func, Self* self)
    {
        route_raw_map_.replace(protocol, sub_invoker_function_raw{ std::bind(&invoker_raw<Function>::template apply_member<Self>, func, self, std::placeholders::_1) });
    }

    void unbind(const std::string& protocol)
    {
        route_map_.erase(protocol);
    }

    bool is_bind(const std::string& protocol)
    {
        return route_map_.exists(protocol);
    }

    void unbind_raw(const std::string& protocol)
    {
        route_raw_map_.erase(protocol);
    }

    bool is_bind_raw(const std::string& protocol)
    {
        return route_raw_map_.exists(protocol);
    }

    bool route(const std::string& protocol, const message_ptr& req)
    {
        sub_invoker_function func;
        if (route_map_.find(protocol, func))
        {
            func(req);
            return true;
        }
        return false;
    }

    bool route_raw(const std::string& protocol, const std::string& req)
    {
        sub_invoker_function_raw func;
        if (route_raw_map_.find(protocol, func))
        {
            func(req);
            return true;
        }
        return false;
    }

    std::vector<std::string> get_all_topic()
    {
        std::vector<std::string> topic_vec;

        route_map_.for_each([&](const std::string& protocol, const sub_invoker_function&)
        {
            topic_vec.emplace_back(protocol);
        });

        route_raw_map_.for_each([&](const std::string& protocol, const sub_invoker_function_raw&)
        {
            topic_vec.emplace_back(protocol);
        });

        return std::move(topic_vec);
    }

private:
    threadsafe_unordered_map<std::string, sub_invoker_function> route_map_;
    threadsafe_unordered_map<std::string, sub_invoker_function_raw> route_raw_map_;
};

}
}

