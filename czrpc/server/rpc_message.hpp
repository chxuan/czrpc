#pragma once

#include "base/serialize_util.hpp"
#include "connection.hpp"

using namespace czrpc::server;

namespace czrpc
{
namespace message
{
class request
{
public:
    request(const message_ptr& message, const std::string& session_id) 
        : message_(message), session_id_(session_id) {}
    request(const std::string& raw_data, const std::string& session_id) 
        : raw_data_(raw_data), session_id_(session_id) {}

    message_ptr message() const { return message_; }
    std::string raw_data() const { return raw_data_; }
    std::string session_id() const { return session_id_; }

private:
    message_ptr message_ = nullptr;
    std::string raw_data_;
    std::string session_id_;
};
using request_ptr = std::shared_ptr<request>;

class response
{
public:
    response(const connection_ptr& conn, unsigned int call_id) : connect_(conn), call_id_(call_id) {}
    void set_response(const message_ptr& message) 
    { 
        if (message != nullptr)
        {
            std::string message_name = message->GetDescriptor()->full_name();
            std::string body = serialize_util::singleton::get()->serialize(message);
            if (!message_name.empty() && !body.empty())
            {
                connect_->async_write(response_content{ call_id_, message_name, body });
            }                    
        }
    }

    void set_response(const std::string& raw_data) 
    {
        if (!raw_data.empty())
        {
            connect_->async_write(response_content{ call_id_, "", raw_data });
        }         
    }

private:
    connection_ptr connect_;
    unsigned int call_id_ = 0;
};
using response_ptr = std::shared_ptr<response>;

}
}
