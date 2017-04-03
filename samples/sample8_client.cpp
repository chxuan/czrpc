/************************************************
 * 发布订阅客户端
************************************************/
#include <iostream>
#include <thread>
#include "czrpc/client/client.hpp"
#include "common.pb.h"

czrpc::client::pub_client client;

void pub_func()
{
    while (true)
    {
        try
        {
            auto message = std::make_shared<news>();
            message->set_str("Good news");

            client.publish("news", message);
            client.publish_raw("song", "My heart will go on");
        }
        catch (std::exception& e)
        {
            std::cout << e.what() << std::endl;
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
        client2.subscribe("news", &auto_news);
        client2.subscribe_raw("song", [](const std::string& str){ std::cout << str << std::endl; });
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    pub_func();

    return 0;
}
