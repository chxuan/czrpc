/************************************************
 * 订阅者
************************************************/
#include <iostream>
#include <thread>
#include "czrpc/client/client.hpp"
#include "common.pb.h"

void auto_news(const message_ptr& in_message)
{
    auto message = std::dynamic_pointer_cast<news>(in_message);
    message->PrintDebugString();
}

int main()
{
    czrpc::client::sub_client client;
    try
    {
        client.connect("127.0.0.1:50051").timeout(3, 3).run();
        client.subscribe("news", &auto_news);
        client.subscribe_raw("song", [](const std::string& str){ std::cout << str << std::endl; });
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
