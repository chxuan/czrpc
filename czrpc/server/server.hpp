#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "base/table/threadsafe_unordered_map.hpp"
#include "base/thread_pool.hpp"
#include "io_service_pool.hpp"
#include "router.hpp"
#include "tcp_endpoint.hpp"
#include "topic_manager.hpp"

using namespace czrpc::base::table;

namespace czrpc
{
namespace server
{
class server
{
public:
    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server() : timer_work_(timer_ios_), timer_(timer_ios_) {}

    ~server()
    {
        stop();
    }

    server& listen(const endpoint& ep)
    {
        endpoint_vec_.emplace_back(ep);
        return *this;
    }

    server& listen(const std::vector<endpoint>& ep_vec)
    {
        endpoint_vec_ = ep_vec;
        return *this;
    }

    server& timeout(std::size_t timeout_milli)
    {
        timeout_milli_ = timeout_milli;
        return *this;
    }

    server& ios_threads(std::size_t num)
    {
        ios_thread_num_ = num;
        return *this;
    }

    server& work_threads(std::size_t num)
    {
        work_thread_num_ = num;
        return *this;
    }

    void run()
    {
        io_service_pool::singleton::get()->multithreaded(ios_thread_num_);
        threadpool_.init_thread_num(work_thread_num_);
        listen();
        accept();
        io_service_pool::singleton::get()->run();
        start_timer_thread();
    }

    void stop()
    {
        stop_timer_thread();
        io_service_pool::singleton::get()->stop();
    }

    void set_client_connect_notify(const std::function<void(const std::string&)>& func)
    {
        client_connect_notify_ = func;
    }

    void set_client_disconnect_nofity(const std::function<void(const std::string&)>& func)
    {
        client_disconnect_notify_ = func;
    }

    template<typename Function>
    void bind(const std::string& protocol, const Function& func)
    {
        router::singleton::get()->bind(protocol, func);
    }

    template<typename Function, typename Self>
    void bind(const std::string& protocol, const Function& func, Self* self)
    {
        router::singleton::get()->bind(protocol, func, self); 
    }

    void unbind(const std::string& protocol)
    {
        router::singleton::get()->unbind(protocol);
    }

    bool is_bind(const std::string& protocol)
    {
        return router::singleton::get()->is_bind(protocol);
    }

    template<typename Function>
    void bind_raw(const std::string& protocol, const Function& func)
    {
        router::singleton::get()->bind_raw(protocol, func);
    }

    template<typename Function, typename Self>
    void bind_raw(const std::string& protocol, const Function& func, Self* self)
    {
        router::singleton::get()->bind_raw(protocol, func, self); 
    }

    void unbind_raw(const std::string& protocol)
    {
        router::singleton::get()->unbind_raw(protocol);
    }

    bool is_bind_raw(const std::string& protocol)
    {
        return router::singleton::get()->is_bind_raw(protocol);
    }

private:
    void listen()
    {
        auto route_func = std::bind(&server::route, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        auto handle_error_func = std::bind(&server::handle_error, this, std::placeholders::_1);
        for (auto& ep : endpoint_vec_)
        {
            auto endpoint = std::make_shared<tcp_endpoint>(route_func, handle_error_func, 
                                                           client_connect_notify_, client_disconnect_notify_);
            endpoint->listen(ep.ip, ep.port);
            tcp_endpoint_vec_.emplace_back(endpoint);
        }
    }

    void accept()
    {
        for (auto& endpoint : tcp_endpoint_vec_)
        {
            endpoint->accept();
        }
    }

    void route(const request_content& content, const client_flag& flag, const connection_ptr& conn)
    {
        threadpool_.add_task(&server::router_thread, this, content, flag, conn);
    }

    void router_thread(const request_content& content, const client_flag& flag, const connection_ptr& conn)
    {
        if (flag.type == client_type::rpc_client || flag.type == client_type::async_rpc_client)
        {
            rpc_coming(content, flag, conn);
        }
        else if (flag.type == client_type::pub_client)
        {
            push_content ctx { content.protocol, content.message_name, content.body };
            publisher_coming(flag.mode, ctx);
        }
        else if (flag.type == client_type::sub_client)
        {
            subscriber_coming(content.protocol, content.body, conn);
        }
    }

    void rpc_coming(const request_content& content, const client_flag& flag, const connection_ptr& conn)
    {
        try
        {
            if (flag.mode == serialize_mode::serialize)
            {
                rpc_coming_with_serialize(content, conn);
            }
            else if (flag.mode == serialize_mode::non_serialize)
            {
                rpc_coming_with_non_serialize(content, conn);
            }
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
            conn->disconnect();
        }
    }

    void rpc_coming_with_serialize(const request_content& content, const connection_ptr& conn)
    {
        message_ptr rsp = nullptr;
        message_ptr req = serialize_util::singleton::get()->deserialize(content.message_name, content.body);
        if (router::singleton::get()->route(content.protocol, conn->get_session_id(), req, rsp))
        {
            if (rsp != nullptr)
            {
                std::string message_name = rsp->GetDescriptor()->full_name();
                std::string body = serialize_util::singleton::get()->serialize(rsp);
                if (!message_name.empty() && !body.empty())
                {
                    conn->async_write(response_content{ content.call_id, message_name, body });
                }                    
            }
        }
        else
        {
            log_warn("Route failed, invaild protocol: {}", content.protocol);
            response_error(content.call_id, rpc_error_code::route_failed, conn);
        }
    }

    void rpc_coming_with_non_serialize(const request_content& content, const connection_ptr& conn)
    {
        std::string rsp;
        if (router::singleton::get()->route_raw(content.protocol, conn->get_session_id(), content.body, rsp))
        {
            if (!rsp.empty())
            {
                conn->async_write(response_content{ content.call_id, "", rsp });
            }               
        }
        else
        {
            log_warn("Route failed, invaild protocol: {}", content.protocol);
            response_error(content.call_id, rpc_error_code::route_failed, conn);
        }
    }

    void publisher_coming(serialize_mode mode, const push_content& content)
    {
        for (auto& conn : topic_manager::singleton::get()->get_connection_by_topic(content.protocol))
        {
            try
            {
                if (!conn.expired())
                {
                    conn.lock()->async_write(mode, content);
                }
            }
            catch (std::exception& e)
            {
                log_warn(e.what());
                conn.lock()->disconnect();
            }
        }
    }

    void subscriber_coming(const std::string& topic_name, const std::string& body, const connection_ptr& conn)
    {
        if (topic_name == heartbeats_flag && body == heartbeats_flag)
        {
            conn_map_.replace(conn, time(nullptr));
        }
        else if (topic_name != heartbeats_flag && body == subscribe_topic_flag)
        {
            conn_map_.replace(conn, time(nullptr));
            topic_manager::singleton::get()->add_topic(topic_name, conn);
        }
        else if (topic_name != heartbeats_flag && body == cancel_subscribe_topic_flag)
        {
            topic_manager::singleton::get()->remove_topic(topic_name, conn);
        }
    }

    void response_error(const std::string& call_id, rpc_error_code code, const connection_ptr& conn)
    {
        response_content content;
        content.call_id = call_id;
        conn->async_write(content, code);
    }
   
    void handle_error(const connection_ptr& conn)
    {
        topic_manager::singleton::get()->remove_all_topic(conn);
        conn_map_.erase(conn);
    }

    void start_timer_thread()
    {
        timer_thread_ = std::make_unique<std::thread>([this]{ timer_ios_.run(); });
        timer_.bind([this]{ check_connection_timeout(); });
        timer_.start(connection_timeout_milli);
    }

    void stop_timer_thread()
    {
        timer_ios_.stop();
        if (timer_thread_ != nullptr)
        {
            if (timer_thread_->joinable())
            {
                timer_thread_->join();
            }
        }
    }

    void check_connection_timeout()
    {
        auto conn_map = conn_map_.clone();
        time_t current_time = time(nullptr);
        for (auto& iter : conn_map)
        {
            if (current_time - iter.second >= connection_timeout_sec)
            {
                iter.first->disconnect();
            }
        }
    }

private:
    std::size_t timeout_milli_ = 0;
    std::size_t ios_thread_num_ = 1;
    std::size_t work_thread_num_ = 1;

    std::vector<endpoint> endpoint_vec_;
    std::vector<std::shared_ptr<tcp_endpoint>> tcp_endpoint_vec_;
    threadsafe_unordered_map<connection_ptr, time_t> conn_map_;

    boost::asio::io_service timer_ios_;
    boost::asio::io_service::work timer_work_;
    std::unique_ptr<std::thread> timer_thread_;
    atimer<> timer_;

    std::function<void(const std::string&)> client_connect_notify_ = nullptr;
    std::function<void(const std::string&)> client_disconnect_notify_ = nullptr;
    thread_pool threadpool_;
};

}
}

