#include <iostream>
#include "czrpc/client/client.hpp"
#include "common.pb.h"
using namespace czrpc::base;

int main()
{   
    czrpc::client::rpc_client client;
    try
    {
        client.connect({ "127.0.0.1", 50051 }).run();
        auto req = std::make_shared<echo_message>();
        req->set_echo_str("Hello world");
        req->set_echo_num(100);
        auto rsp = client.call("echo", req);
        rsp->PrintDebugString();
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    std::cin.get();
    return 0;
}
