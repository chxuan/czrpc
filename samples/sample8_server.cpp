/************************************************
 * 作为发布订阅服务端
************************************************/
#include <iostream>
#include "czrpc/server/server.hpp"

using namespace czrpc::base;

int main()
{
    // 1.创建rpc服务器对象
    czrpc::server::server server;
    try
    {
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
