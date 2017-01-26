#include <iostream>
#include "czrpc/client/client.hpp"
#include "proto_message.pb.h"

using namespace czrpc::base;

void auto_weather(const std::shared_ptr<google::protobuf::Message>& in_message)
{
    in_message->PrintDebugString();
}

void auto_song(const std::string& str)
{
    std::cout << str << std::endl;
}

int main()
{
    try
    {
        czrpc::client::sub_client client;
        client.connect({ "127.0.0.1", 50051 }).timeout(3000).run();
        client.async_subscribe("weather", &auto_weather);
        client.async_subscribe_raw("song", [](const std::string& str){ std::cout << str << std::endl; });
        std::cin.get();
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    return 0;
}
