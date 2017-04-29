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
            auto message = std::make_shared<request_person_info_message>();
            message->set_name("Jack");
            message->set_age(20);

            client.async_call("request_person_info", message).result([](const czrpc::message::result_ptr& ret)
            {
                if (ret->error_code())
                {
                    log_warn() << ret->error_code().message();
                    return;
                }

                if (IS_SAME(ret->message(), response_person_info_message))
                {
                    auto message = std::dynamic_pointer_cast<response_person_info_message>(ret->message()); 
                    message->PrintDebugString();
                }
                else if (IS_SAME(ret->message(), response_error))
                {
                    auto message = std::dynamic_pointer_cast<response_error>(ret->message()); 
                    message->PrintDebugString();
                }
            });

            client.async_call_raw("echo", "Hello world").result([](const czrpc::message::result_ptr& ret)
            {
                if (ret->error_code())
                {
                    log_warn() << ret->error_code().message();
                    return;
                }
                std::cout << ret->raw_data() << std::endl;
            });
        }
        catch (std::exception& e)
        {
            log_warn() << e.what();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void connect_success_notify()
{
    log_info() << "connect success...";
}

int main()
{   
    try
    {
        client.set_connect_success_notify(std::bind(&connect_success_notify));
        client.connect({ "127.0.0.1", 50051 }).timeout(3000).run();
    }
    catch (std::exception& e)
    {
        log_warn() << e.what();
        return 0;
    }

#if 0
    try
    {
        auto message = std::make_shared<request_person_info_message>();
        message->set_name("Jack");
        message->set_age(20);

        client.async_call("request_person_info", message).result([](const message_ptr& in_message, const czrpc::base::error_code& ec)
        {
            if (ec)
            {
                log_warn() << ec.message();
                return;
            }

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
        log_warn() << e.what();
        return 0;
    }
    std::cin.get();

#else
    std::thread t(test_func);
    std::thread t2(test_func);

    t.join();
    t2.join();
#endif

    return 0;
}
