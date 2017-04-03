/************************************************
 * 带session id的rpc服务端
************************************************/
#include <iostream>
#include "czrpc/server/server.hpp"
#include "common.pb.h"

void echo(const czrpc::message::request_ptr& req, const czrpc::message::response_ptr& rsp)
{
    std::cout << "session_id: " << req->session_id() << std::endl;
    req->message()->PrintDebugString();
    rsp->set_response(req->message());
}

void client_connect_notify(const std::string& session_id)
{
    std::cout << "connect session id: " << session_id  << std::endl;
}

void client_disconnect_notify(const std::string& session_id)
{
    std::cout << "disconnect session id: " << session_id  << std::endl;
}

int main()
{
    // 1.创建rpc服务器对象
    czrpc::server::server server;
    try
    {
        // 2.设置客户端连接、断开提醒
        server.set_client_connect_notify(std::bind(&client_connect_notify, std::placeholders::_1));
        server.set_client_disconnect_nofity(std::bind(&client_disconnect_notify, std::placeholders::_1));

        // 3.绑定echo函数
        server.bind("echo", &echo);

        // 4.配置监听参数并启动事件循环（非阻塞）
        // 服务端默认启动一个ios线程和一个work线程
        server.listen({ "127.0.0.1", 50051 }).run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
