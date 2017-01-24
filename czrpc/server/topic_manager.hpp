#pragma once

#include <string>
#include <unordered_map>
#include "base/singleton.hpp"
#include "connection.hpp"

namespace czrpc
{
namespace server
{
class topic_manager
{
    DEFINE_SINGLETON(topic_manager);
public:
    topic_manager() = default;
    void add_topic(const std::string& topic_name, const connection_ptr& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto range = topic_map_.equal_range(topic_name);
        for (auto iter = range.first; iter != range.second; ++iter)
        {
            if (iter->second.lock() == conn)
            {
                return;
            }
        }
        topic_map_.emplace(topic_name, conn);
    }

    void remove_topic(const std::string& topic_name, const connection_ptr& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = topic_map_.find(topic_name);
        if (iter != topic_map_.end())
        {
            auto range = topic_map_.equal_range(iter->first);
            while (range.first != range.second)
            {
                if (range.first->second.lock() == conn)
                {
                    range.first = topic_map_.erase(range.first);
                }
                else
                {
                    ++range.first;
                }
            }
        }
    }

    std::vector<connection_weak_ptr> get_connection_by_topic(const std::string& topic_name)
    {
        std::vector<connection_weak_ptr> conn_vec;
        std::lock_guard<std::mutex> lock(mutex_);
        auto range = topic_map_.equal_range(topic_name);
        for (auto iter = range.first; iter != range.second; ++iter)
        {
            conn_vec.emplace_back(iter->second);
        }
        return std::move(conn_vec);
    }

    void remove_all_topic(const connection_ptr& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto begin = topic_map_.begin();
        while (begin != topic_map_.end())
        {
            if (begin->second.lock() == conn)
            {
                std::cout << "remove topic: " << begin->first << std::endl;
                begin = topic_map_.erase(begin);
            }
            else
            {
                ++begin;
            }
        }
    }

private:
    std::unordered_multimap<std::string, connection_weak_ptr> topic_map_;
    std::mutex mutex_;
};

}
}

