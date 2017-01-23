#ifndef _HEADER_H
#define _HEADER_H

#include <string>

namespace easyrpc
{

constexpr const int max_buffer_len = 20 * 1024 * 1024; // 20MB
constexpr const int request_header_len = 4 + 4 + 4 + 4 + 4;
constexpr const int response_header_len = 4 + 4;
constexpr const int push_header_len = 4 + 4 + 4;
const std::string subscribe_topic_flag = "1";
const std::string cancel_subscribe_topic_flag = "0";
const std::string heartbeats_flag = "00";
const int heartbeats_milli = 3000;
const int connection_timeout_milli = 30000;
const int connection_timeout_sec = 30;

enum class serialize_mode : unsigned int
{
    serialize,
    non_serialize
};

enum class client_type : unsigned int
{
    rpc_client,
    pub_client,
    sub_client
};

struct client_flag
{
    serialize_mode mode;
    client_type type;
};

struct request_header
{
    unsigned int call_id_len;
    unsigned int protocol_len;
    unsigned int body_len;
    client_flag flag;
};

struct request_content
{
    std::string call_id;
    std::string protocol;
    std::string body;
};

struct request_data
{
    request_header header;
    request_content content;
};

struct response_header
{
    unsigned int call_id_len;
    unsigned int body_len;
};

struct response_content
{
    std::string call_id;
    std::string body;
};

struct response_data
{
    response_header header;
    response_content content;
};

struct push_header
{
    unsigned int protocol_len;
    unsigned int body_len;
    serialize_mode mode;
};

struct push_content
{
    std::string protocol;
    std::string body;
};

struct push_data
{
    push_header header;
    push_content content;
};

struct endpoint
{
    std::string ip;
    unsigned short port;
};

using one_way = void;
using two_way = std::string;

}

#endif
