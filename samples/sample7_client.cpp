/************************************************
 * RPC异步调用
************************************************/
#include <iostream>
#include "czrpc/client/client.hpp"
#include "common.pb.h"
using namespace czrpc::base;

int main()
{   
    // 1.创建rpc客户端对象
    czrpc::client::async_rpc_client client;
    try
    {
        // 2.配置连接参数并启动事件循环（非阻塞）
        client.connect({ "127.0.0.1", 50051 }).run();

        auto req = std::make_shared<echo_message>();
        req->set_echo_str("Hello world");
        req->set_echo_num(100);

        // 3.异步调用echo函数
        client.async_call("echo", req).result([](const message_ptr& rsp, const czrpc::base::error_code& ec)
        {
            if (ec)
            {
                log_warn(ec.message());
                return;
            }
            rsp->PrintDebugString();
        });
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    std::cin.get();
    return 0;
}
