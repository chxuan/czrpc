#pragma once

#include "base/header.hpp"
#include "base/serialize_util.hpp"

namespace czrpc
{
namespace message
{
class result
{
public:
    result(const czrpc::base::error_code& ec, unsigned int call_id, const message_ptr& message = nullptr) 
        : ec_(ec), call_id_(call_id), message_(message) {}
    result(const czrpc::base::error_code& ec, unsigned int call_id, const std::string& raw_data) 
        : ec_(ec), call_id_(call_id), raw_data_(raw_data) {}

    message_ptr message() const { return message_; }
    std::string raw_data() const { return raw_data_; }
    unsigned int call_id() const { return call_id_; }
    czrpc::base::error_code error_code() const { return ec_; }

private:
    czrpc::base::error_code ec_;
    unsigned int call_id_ = 0;
    message_ptr message_ = nullptr;
    std::string raw_data_;
};
using result_ptr = std::shared_ptr<result>;

}
}
