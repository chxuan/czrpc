#pragma once

#include <string>
#include <unordered_map>
#include "base/singleton.hpp"
#include "connection.hpp"

namespace czrpc
{
namespace server
{
class connection_manager
{
    DEFINE_SINGLETON(connection_manager);
public:
    connection_manager() = default;
    void add_connection(const connection_ptr& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn_map_.emplace(conn, time(nullptr));
    }

    void remove_connection(const connection_ptr& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "remove_connection" << std::endl;
        conn_map_.erase(conn);
    }

    void update_time(const connection_ptr& conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = conn_map_.find(conn);
        if (iter != conn_map_.end())
        {
            std::cout << "current time: " << time(nullptr) << std::endl;
            iter->second = time(nullptr);
        }
    }

    std::unordered_map<connection_ptr, time_t> get_connection_map()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return conn_map_;
    }

private:
    std::unordered_map<connection_ptr, time_t> conn_map_;
    std::mutex mutex_;
};

}
}

