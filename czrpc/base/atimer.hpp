#pragma once

#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <functional>
#include <boost/timer.hpp>
#include <boost/asio.hpp>

namespace czrpc
{
namespace base
{
template<typename Duration = boost::posix_time::milliseconds>
class atimer
{
public:
    atimer() : work_(ios_), timer_(ios_), is_single_shot_(false) {}
    ~atimer()
    {
        destroy();
    }

    void start(std::size_t duration)
    {
        if (ios_.stopped() || duration == 0)
        {
            return;
        }

        duration_ = duration;
        if (thread_ == nullptr)
        {
            thread_ = std::make_unique<std::thread>([this]{ ios_.run(); });
        }
        start();
    }

    void start()
    {
        if (duration_ == 0)
        {
            return;
        }

        timer_.expires_from_now(Duration(duration_));
        timer_.async_wait([this](const boost::system::error_code& ec)
        {
            if (ec)
            {
                return;
            }

            for (auto& func : func_vec_)
            {
                func();
            }

            if (!is_single_shot_)
            {
                start();
            }
        });
    }

    void stop()
    {
        timer_.cancel();
    }

    void destroy()
    {
        stop();
        ios_.stop();
        if (thread_ != nullptr)
        {
            if (thread_->joinable())
            {
                thread_->join();
            }
        }
    }

    void bind(const std::function<void()>& func)
    {
        func_vec_.emplace_back(func);
    }

    void set_single_shot(bool is_single_short)
    {
        is_single_shot_ = is_single_short; 
    }

private:
    boost::asio::io_service ios_;
    boost::asio::io_service::work work_;
    boost::asio::deadline_timer timer_;
    std::unique_ptr<std::thread> thread_ = nullptr;
    std::vector<std::function<void()>> func_vec_;
    std::atomic<bool> is_single_shot_;
    std::size_t duration_ = 0;
};

}
}

