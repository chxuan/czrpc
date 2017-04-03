#include <iostream>
#include "czrpc/server/server.hpp"
#include "proto_message.pb.h"

void request_person_info(const czrpc::message::request_ptr& req, const czrpc::message::response_ptr& rsp)
{
    std::cout << "session id: " << req->session_id() << std::endl;
    auto message = std::dynamic_pointer_cast<request_person_info_message>(req->message());
    message->PrintDebugString();
#if 1
    auto out_message = std::make_shared<response_person_info_message>();
    out_message->set_name("Tom");
    out_message->set_age(21);
#else
    auto out_message = std::make_shared<response_error>();
    out_message->set_error_code(100);
    out_message->set_error_string("Not found person info");
#endif
    rsp->set_response(out_message);
}

class test
{
public:
    void echo(const czrpc::message::request_ptr& req, const czrpc::message::response_ptr& rsp)
    {
        rsp->set_response(req->raw_data());
    }
};

void client_connect_notify(const std::string& session_id)
{
    std::cout << "connect session id: " << session_id << std::endl;
}

void client_disconnect_notify(const std::string& session_id)
{
    std::cout << "disconnect session id: " << session_id << std::endl;
}

int main()
{
    czrpc::server::server server;
    test t;
    try
    {
        server.set_client_connect_notify(std::bind(&client_connect_notify, std::placeholders::_1));
        server.set_client_disconnect_nofity(std::bind(&client_disconnect_notify, std::placeholders::_1));
        server.bind("request_person_info", &request_person_info);
        server.bind_raw("echo", &test::echo, &t);

        std::vector<czrpc::base::endpoint> ep;
        ep.emplace_back(czrpc::base::endpoint{ "0.0.0.0", 50051 });
        ep.emplace_back(czrpc::base::endpoint{ "0.0.0.0", 50052 });
        server.listen(ep).ios_threads(std::thread::hardware_concurrency()).work_threads(10).run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
