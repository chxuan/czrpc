#include <iostream>
#include "czrpc/client/client.hpp"
#include "proto_message.pb.h"

void auto_weather(const std::shared_ptr<google::protobuf::Message>& in_message)
{
    auto message = std::dynamic_pointer_cast<auto_weather_message>(in_message);
    message->PrintDebugString();
}

void auto_song(const std::string& str)
{
    std::cout << str << std::endl;
}

void connect_success_notify()
{
    log_info() << "connect success...";
}

int main()
{
    try
    {
        czrpc::client::sub_client client;
        client.set_connect_success_notify(std::bind(&connect_success_notify));
        client.connect({ "127.0.0.1", 50051 }).timeout(3, 3).run();
        client.subscribe("weather", &auto_weather);
        client.subscribe_raw("song", [](const std::string& str){ std::cout << str << std::endl; });
        std::cin.get();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    return 0;
}
