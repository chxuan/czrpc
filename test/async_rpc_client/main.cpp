#include <iostream>
#include "czrpc/client/client.hpp"
#include "proto_message.pb.h"

using namespace czrpc::base;

#define IS_SAME(message, other_message) (message->GetDescriptor()->full_name() == other_message::descriptor()->full_name())

czrpc::client::async_rpc_client client;

void test_func()
{
    while (true)
    {
        try
        {
            auto in_message = std::make_shared<request_person_info_message>();
            in_message->set_name("Jack");
            in_message->set_age(20);
#if 0
            client.async_call("request_person_info11", in_message).result([](const auto& in_message)
            {
                if (IS_SAME(in_message, response_person_info_message))
                {
                    auto message = std::dynamic_pointer_cast<response_person_info_message>(in_message); 
                    message->PrintDebugString();
                }
                else if (IS_SAME(in_message, response_error))
                {
                    auto message = std::dynamic_pointer_cast<response_error>(in_message); 
                    message->PrintDebugString();
                }
            });
#endif

            client.async_call("request_person_info", in_message).result([](const auto& in_message)
            {
                if (IS_SAME(in_message, response_person_info_message))
                {
                    auto message = std::dynamic_pointer_cast<response_person_info_message>(in_message); 
                    message->PrintDebugString();
                }
                else if (IS_SAME(in_message, response_error))
                {
                    auto message = std::dynamic_pointer_cast<response_error>(in_message); 
                    message->PrintDebugString();
                }
            });
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main()
{   
    try
    {
        client.connect({ "127.0.0.1", 50051 }).timeout(3000).run();
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    std::thread t(test_func);
    std::thread t2(test_func);

    t.join();
    t2.join();

    return 0;
}
