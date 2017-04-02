/************************************************
 * 高性能的rpc服务端
************************************************/
#include <iostream>
#include "czrpc/server/server.hpp"
#include "common.pb.h"

void echo(const czrpc::message::request_ptr& req, const czrpc::message::response_ptr& rsp)
{
    req->message()->PrintDebugString();
    rsp->set_response(req->message());
}

int main()
{
    // 1.创建rpc服务器对象
    czrpc::server::server server;
    try
    {
        // 2.绑定echo函数
        server.bind("echo", &echo);

        // 3.设置ios线程数为CPU核数
        // 设置10个work线程
        // 并启动事件循环（非阻塞）
        server.listen({ "127.0.0.1", 50051 }).ios_threads(std::thread::hardware_concurrency()).work_threads(10).run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
