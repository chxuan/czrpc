#include <iostream>
#include "czrpc/server/server.hpp"
#include "common.pb.h"

using namespace czrpc::base;
using message_ptr = std::shared_ptr<google::protobuf::Message>;

message_ptr echo(const message_ptr& req)
{
    req->PrintDebugString();
    return req;
}

int main()
{
    czrpc::server::server server;
    try
    {
        server.bind("echo", &echo);
        server.listen({ "127.0.0.1", 50051 }).run();
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    std::cin.get();
    return 0;
}
