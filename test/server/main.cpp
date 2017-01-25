#include <iostream>
#include "czrpc/server/server.hpp"
#include "proto_message.pb.h"

using namespace czrpc::base;

std::shared_ptr<google::protobuf::Message> request_person_info(const std::shared_ptr<google::protobuf::Message>& in_message)
{
    in_message->PrintDebugString();
#if 0
    auto out_message = std::make_shared<response_person_info_message>();
    out_message->set_name("Tom");
    out_message->set_age(21);
#endif
    auto out_message = std::make_shared<response_error>();
    out_message->set_error_code(100);
    out_message->set_error_string("Not found person info");
    return out_message;
}

int main()
{
    czrpc::server::server server;
    try
    {
        server.bind("request_person_info", &request_person_info);
        /* server.bind_raw("request_person_info", &request_person_info); */

        std::vector<czrpc::base::endpoint> ep;
        ep.emplace_back(czrpc::base::endpoint{ "127.0.0.1", 50051 });
        ep.emplace_back(czrpc::base::endpoint{ "127.0.0.1", 50052 });
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
