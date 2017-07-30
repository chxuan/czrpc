/************************************************
 * 带超时的rpc客户端
************************************************/
#include <iostream>
#include "czrpc/client/client.hpp"
#include "common.pb.h"

int main()
{   
    // 1.创建rpc客户端对象
    czrpc::client::rpc_client client;
    try
    {
        // 2.配置连接参数
        // 设置连接超时3秒，请求超时10秒
        // 启动事件循环（非阻塞）
        client.connect("127.0.0.1:50051").timeout(3, 10).run();

        auto req = std::make_shared<echo_message>();
        req->set_echo_str("Hello world");
        req->set_echo_num(100);

        // 3.同步调用echo函数
        auto rsp = client.call("echo", req);

        rsp->PrintDebugString();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
