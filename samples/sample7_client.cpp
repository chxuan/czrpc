/************************************************
 * RPC异步调用
************************************************/
#include <iostream>
#include "czrpc/client/client.hpp"
#include "common.pb.h"

int main()
{   
    // 1.创建rpc客户端对象
    czrpc::client::async_rpc_client client;
    try
    {
        // 2.resend置为true：当网络断开或服务器关闭时底层不会丢数据直到连接成功后再将之前的数据一并放到服务器，默认为关闭该功能
        client.connect({ "127.0.0.1", 50051 }).resend(true).run();

        auto req = std::make_shared<echo_message>();
        req->set_echo_str("Hello world");
        req->set_echo_num(100);

        // 3.异步调用echo函数
        client.async_call("echo", req).result([](const czrpc::message::result_ptr& ret)
        {
            if (ret->error_code())
            {
                std::cout << ret->error_code().message() << std::endl;
                return;
            }
            ret->message()->PrintDebugString();
        });
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
