/************************************************
 * 绑定类成员函数的rpc服务端
************************************************/
#include <iostream>
#include "czrpc/server/server.hpp"
#include "common.pb.h"

class test
{
public:
    void echo(const czrpc::message::request_ptr& req, const czrpc::message::response_ptr& rsp)
    {
        std::cout << "session_id: " << req->session_id() << std::endl;
        req->message()->PrintDebugString();
        rsp->set_response(req->message());
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
        // 3.绑定类成员函数
        server.bind("echo", &test::echo, &t);

        // 4.配置监听参数并启动事件循环（非阻塞）
        // 服务端默认启动一个ios线程和一个work线程
        server.listen("127.0.0.1:50051").run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
