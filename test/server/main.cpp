#include <iostream>
#include <thread>
#include "czrpc/server/server.hpp"
#include "proto_message.pb.h"

czrpc::server::server app;

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

void test_func()
{
    while (true)
    {
        try
        {
            auto message = std::make_shared<auto_weather_message>();
            message->set_city_name("ChengDu");
            message->set_weather("Good");
            app.publish("weather", message);
            app.publish_raw("song", "My heart will go on");
        }
        catch (std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main()
{
    test t;
    try
    {
        app.set_client_connect_notify(std::bind(&client_connect_notify, std::placeholders::_1));
        app.set_client_disconnect_nofity(std::bind(&client_disconnect_notify, std::placeholders::_1));
        app.bind("request_person_info", &request_person_info);
        app.bind_raw("echo", &test::echo, &t);

        std::vector<czrpc::base::endpoint> ep;
        ep.emplace_back(czrpc::base::endpoint{ "0.0.0.0", 50051 });
        ep.emplace_back(czrpc::base::endpoint{ "0.0.0.0", 50052 });
        app.listen(ep).ios_threads(std::thread::hardware_concurrency()).work_threads(10).run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::thread t1(test_func);
    std::thread t2(test_func);

    t1.join();
    t2.join();

    std::cin.get();
    return 0;
}
