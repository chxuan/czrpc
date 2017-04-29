#pragma once

#include "base/header.hpp"
#include "base/serialize_util.hpp"
#include "base/czlog.hpp"
#include "result.hpp"

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

    unsigned int result(const std::function<void(const czrpc::message::result_ptr&)>& func)
    {
        task_ = [func, this](const response_content& content)
        {
            try
            {
                czrpc::base::error_code ec(content.code);
                if (!content.message_name.empty())
                {
                    if (ec)
                    {
                        auto ret = std::make_shared<czrpc::message::result>(ec, content.call_id);
                        func(ret);
                    }
                    else
                    {
                        auto ret = std::make_shared<czrpc::message::result>(ec, content.call_id,
                                                                            serialize_util::singleton::get()->deserialize(content.message_name, content.body));
                        func(ret);
                    }
                }
                else
                {
                    if (ec)
                    {
                        auto ret = std::make_shared<czrpc::message::result>(ec, content.call_id, "");
                        func(ret);
                    }
                    else
                    {
                        auto ret = std::make_shared<czrpc::message::result>(ec, content.call_id, content.body);
                        func(ret);
                    }
                }
            }
            catch (std::exception& e)
            {
                log_warn() << e.what();
            }
        };
        client_->add_bind_func(content_.call_id, task_);
        client_->async_write(content_);
        return content_.call_id;
    }

private:
    client_flag flag_;
    request_content content_;
    task_t task_;
    T* client_;
};

}
}

