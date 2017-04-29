/************************************************
 * 发布者
************************************************/
#include <iostream>
#include <thread>
#include "czrpc/server/server.hpp"
#include "common.pb.h"

// 1.创建rpc服务器对象
czrpc::server::server pub_server;

void pub_func()
{
    while (true)
    {
        try
        {
            auto message = std::make_shared<news>();
            message->set_str("Good news");

            pub_server.publish("news", message);
            pub_server.publish_raw("song", "My heart will go on");
        }
        catch (std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

int main()
{
    try
    {
        // 4.配置监听参数并启动事件循环（非阻塞）
        // 服务端默认启动一个ios线程和一个work线程
        pub_server.listen({ "127.0.0.1", 50051 }).run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    pub_func();

    std::cin.get();
    return 0;
}
