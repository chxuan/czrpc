/************************************************
 * 最简单的rpc服务端
************************************************/
#include <iostream>
#include "czrpc/server/server.hpp"
#include "common.pb.h"

using namespace czrpc::base;
using message_ptr = std::shared_ptr<google::protobuf::Message>;

message_ptr echo(const message_ptr& req, const std::string& session_id)
{
    std::cout << "session_id: " << session_id << std::endl;
    req->PrintDebugString();
    return req;
}

void client_connect_notify(const std::string& session_id)
{
    log_info("connect session id: {}", session_id);
}

void client_disconnect_notify(const std::string& session_id)
{
    log_info("disconnect session id: {}", session_id);
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
        log_warn(e.what());
        return 0;
    }

    std::cin.get();
    return 0;
}
