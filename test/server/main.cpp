#include <iostream>
#include "czrpc/server/server.hpp"
#include "proto_message.pb.h"

using namespace czrpc::base;

#if 1
std::shared_ptr<google::protobuf::Message> request_person_info(const std::shared_ptr<google::protobuf::Message>& in_message)
{
    in_message->PrintDebugString();
    auto out_message = std::make_shared<response_person_info_message>();
    out_message->set_name("Tom");
    out_message->set_age(21);
    return out_message;
}
#endif

#if 0
std::string request_person_info(const std::string& in_message)
{
    return in_message;
}
#endif

int main()
{
    czrpc::server::server server;
    try
    {
        server.bind("request_person_info", &request_person_info);
        /* server.bind_raw("request_person_info", &request_person_info); */

        std::vector<czrpc::server::endpoint> ep;
        ep.emplace_back(czrpc::server::endpoint{ "127.0.0.1", 50051 });
        ep.emplace_back(czrpc::server::endpoint{ "127.0.0.1", 50052 });
        server.listen(ep).multithreaded(10).run();
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    std::cin.get();
    return 0;
}
