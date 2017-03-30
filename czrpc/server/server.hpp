#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <mutex>
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
    server() = default;
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
    }

    void stop()
    {
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
        auto route_func = std::bind(&server::route, this, std::placeholders::_1, std::placeholders::_2);
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

    void route(const request_content& content, const connection_ptr& conn)
    {
        threadpool_.add_task(&server::router_thread, this, content, conn);
    }

    void router_thread(const request_content& content, const connection_ptr& conn)
    {
        client_type type = content.flag.type;
        if (type == client_type::rpc_client || type == client_type::async_rpc_client)
        {
            rpc_coming(content, conn);
        }
        else if (type == client_type::pub_client)
        {
            push_content ctx { content.protocol, content.message_name, content.body };
            publisher_coming(content.flag.mode, ctx);
        }
        else if (type == client_type::sub_client)
        {
            subscriber_coming(content.protocol, content.body, conn);
        }
    }

    void rpc_coming(const request_content& content, const connection_ptr& conn)
    {
        try
        {
            if (content.flag.mode == serialize_mode::serialize)
            {
                rpc_coming_with_serialize(content, conn);
            }
            else if (content.flag.mode == serialize_mode::non_serialize)
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
        message_ptr req_message = serialize_util::singleton::get()->deserialize(content.message_name, content.body);
        auto req = std::make_shared<request>(req_message, conn->get_session_id());
        auto rsp = std::make_shared<response>(conn, content.call_id);
        if (!router::singleton::get()->route(content.protocol, req, rsp))
        {
            log_warn("Route failed, invaild protocol: {}", content.protocol);
            response_error(content.call_id, rpc_error_code::route_failed, conn);
        }
    }

    void rpc_coming_with_non_serialize(const request_content& content, const connection_ptr& conn)
    {
        auto req = std::make_shared<request>(content.body, conn->get_session_id());
        auto rsp = std::make_shared<response>(conn, content.call_id);
        if (!router::singleton::get()->route_raw(content.protocol, req, rsp))
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
            // do nothing.
        }
        else if (topic_name != heartbeats_flag && body == subscribe_topic_flag)
        {
            topic_manager::singleton::get()->add_topic(topic_name, conn);
        }
        else if (topic_name != heartbeats_flag && body == cancel_subscribe_topic_flag)
        {
            topic_manager::singleton::get()->remove_topic(topic_name, conn);
        }
    }

    void response_error(unsigned int call_id, rpc_error_code code, const connection_ptr& conn)
    {
        response_content content;
        content.call_id = call_id;
        content.code = code;
        conn->async_write(content);
    }
   
    void handle_error(const connection_ptr& conn)
    {
        topic_manager::singleton::get()->remove_all_topic(conn);
    }

private:
    std::size_t timeout_milli_ = 0;
    std::size_t ios_thread_num_ = 1;
    std::size_t work_thread_num_ = 1;

    std::vector<endpoint> endpoint_vec_;
    std::vector<std::shared_ptr<tcp_endpoint>> tcp_endpoint_vec_;

    std::function<void(const std::string&)> client_connect_notify_ = nullptr;
    std::function<void(const std::string&)> client_disconnect_notify_ = nullptr;
    thread_pool threadpool_;
};

}
}

