/************************************************
 * rpc服务端的原始绑定接口
************************************************/
#include <iostream>
#include "czrpc/server/server.hpp"
using namespace czrpc::base;

void echo(const czrpc::message::request_ptr& req, const czrpc::message::response_ptr& rsp)
{
    std::cout << req->raw_data() << std::endl;
    rsp->set_raw_data(req->raw_data());
}

int main()
{
    // 1.创建rpc服务器对象
    czrpc::server::server server;
    try
    {
        // 2.绑定echo函数
        server.bind_raw("echo", &echo);

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
