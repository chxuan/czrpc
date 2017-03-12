/************************************************
 * 发布订阅客户端
************************************************/
#include <iostream>
#include <thread>
#include "czrpc/client/client.hpp"
#include "common.pb.h"

using namespace czrpc::base;
using message_ptr = std::shared_ptr<google::protobuf::Message>;

czrpc::client::pub_client client;

void pub_func()
{
    while (true)
    {
        try
        {
            auto message = std::make_shared<news>();
            message->set_str("Good news");

            // 同步发布
            /* client.publish("news", message); */
            /* client.publish_raw("song", "My heart will go on"); */

            // 异步发布
            client.async_publish("news", message);
            client.async_publish_raw("song", "My heart will go on");
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void auto_news(const message_ptr& in_message)
{
    auto message = std::dynamic_pointer_cast<news>(in_message);
    message->PrintDebugString();
}

int main()
{
    czrpc::client::sub_client client2;
    try
    {
        client.connect({ "127.0.0.1", 50051 }).timeout(3000).run();
        client2.connect({ "127.0.0.1", 50051 }).timeout(3000).run();
        client2.async_subscribe("news", &auto_news);
        client2.async_subscribe_raw("song", [](const std::string& str){ std::cout << str << std::endl; });
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    pub_func();

    return 0;
}
