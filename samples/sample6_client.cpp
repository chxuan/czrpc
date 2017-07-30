/************************************************
 * rpc客户端的原始调用接口
************************************************/
#include <iostream>
#include "czrpc/client/client.hpp"

int main()
{   
    // 1.创建rpc客户端对象
    czrpc::client::rpc_client client;
    try
    {
        // 2.配置连接参数并启动事件循环（非阻塞）
        client.connect("127.0.0.1:50051").run();

        // 3.同步调用echo函数
        std::string rsp = client.call_raw("echo", "Hello czrpc");
        std::cout << rsp << std::endl;
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
