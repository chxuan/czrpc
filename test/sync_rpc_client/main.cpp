#include <iostream>
#include "czrpc/client/client.hpp"
#include "proto_message.pb.h"

using namespace czrpc::base;

#define IS_SAME(message, other_message) (message->GetDescriptor()->full_name() == other_message::descriptor()->full_name())

int main()
{   
    try
    {
        czrpc::client::rpc_client client;
        client.connect({ "127.0.0.1", 50051 }).timeout(3000).run();
        auto in_message = std::make_shared<request_person_info_message>();
        in_message->set_name("Jack");
        in_message->set_age(20);
        auto out_message = client.call("request_person_info", in_message);
        if (IS_SAME(out_message, response_person_info_message))
        {
            auto message = std::dynamic_pointer_cast<response_person_info_message>(out_message); 
            message->PrintDebugString();
        }
        else if (IS_SAME(out_message, response_error))
        {
            auto message = std::dynamic_pointer_cast<response_error>(out_message); 
            message->PrintDebugString();
        }
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    return 0;
}
