/************************************************
 * 绑定类成员函数的rpc服务端
************************************************/
#include <iostream>
#include "czrpc/server/server.hpp"
#include "common.pb.h"

using namespace czrpc::base;
using message_ptr = std::shared_ptr<google::protobuf::Message>;

class test
{
public:
    message_ptr echo(const message_ptr& req)
    {
        req->PrintDebugString();
        return req;
    }
};

int main()
{
    // 1.创建rpc服务器对象
    czrpc::server::server server;
    // 2.创建一个handler
    test t;

    try
    {
        // 2.绑定类成员函数
        server.bind("echo", &test::echo, &t);

        // 3.配置监听参数并启动事件循环（非阻塞）
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
