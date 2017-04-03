#pragma once

#include "base/header.hpp"
#include "base/serialize_util.hpp"

using namespace czrpc::base;

namespace czrpc
{
namespace client
{
using task_t = std::function<void(const response_content&)>; 
template<typename T>
class rpc_task
{
public:
    rpc_task(const request_content& content, T* client) 
        : content_(content), client_(client) {}

    void result(const std::function<void(const message_ptr&, const czrpc::base::error_code&)>& func)
    {
        task_ = [func, this](const response_content& content)
        {
            try
            {
                czrpc::base::error_code ec(content.code);
                if (ec)
                {
                    func(nullptr, ec);
                }
                else
                {
                    func(serialize_util::singleton::get()->deserialize(content.message_name, content.body), ec);
                }
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << std::endl;
            }
        };
        client_->add_bind_func(content_.call_id, task_);
        client_->async_write(content_);
    }

    void result(const std::function<void(const std::string&, const czrpc::base::error_code&)>& func)
    {
        task_ = [func, this](const response_content& content)
        {
            try
            {
                czrpc::base::error_code ec(content.code);
                if (ec)
                {
                    func("", ec);
                }
                else
                {
                    func(content.body, ec);
                }
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << std::endl;
            }
        };
        client_->add_bind_func(content_.call_id, task_);
        client_->async_write(content_);
    }

private:
    client_flag flag_;
    request_content content_;
    task_t task_;
    T* client_;
};

}
}

