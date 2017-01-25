#include <iostream>
#include "czrpc/client/client.hpp"
#include "proto_message.pb.h"

using namespace czrpc::base;

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
        if (out_message->GetDescriptor()->full_name() == response_person_info_message::descriptor()->full_name())
        {
            auto message = std::dynamic_pointer_cast<response_person_info_message>(out_message); 
            message->PrintDebugString();
        }
        else if (out_message->GetDescriptor()->full_name() == response_error::descriptor()->full_name())
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
