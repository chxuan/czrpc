#pragma once

#include <string>
#include <list>
#include <mutex>

namespace czrpc
{
namespace base
{
class async_send_queue
{
public:
    void emplace_back(const std::string& buffer)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        send_queue_.emplace_back(buffer);
    }

    std::string front()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return send_queue_.front();
    }

    void pop_front()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        send_queue_.pop_front();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        send_queue_.clear();
    }

    bool empty()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return send_queue_.empty();
    }

    std::size_t size()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return send_queue_.size();
    }

private:
    std::list<std::string> send_queue_;
    std::mutex mutex_;
};

}
}
