A modern RPC framework based on protobuf 
===============================================

> czrpc是一个纯c++14开发，header-only，使用方便的RPC库。

![License][1] 

## Getting started
首先下载czrpc：

    git clone https://github.com/chxuan/czrpc.git

然后下载依赖的第三方库：

    git submodule update --init --recursive
    
下载过后包含czrpc头文件即可使用。

## Tutorial

* **Simple server**

    ```cpp
    #include <iostream>
    #include "czrpc/server/server.hpp"
    #include "common.pb.h"
    
    void echo(const czrpc::message::request_ptr& req, const czrpc::message::response_ptr& rsp)
    {
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
    
            // 3.配置监听参数并启动事件循环（非阻塞）
            // 服务端默认启动一个ios线程和一个work线程
            server.listen({ "127.0.0.1", 50051 }).run();
        }
        catch (std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return 0;
        }
    
        std::cin.get();
        return 0;
    }
    ```
    
* **Simple client**
    ```cpp
    #include <iostream>
    #include "czrpc/client/client.hpp"
    #include "common.pb.h"
    
    int main()
    {   
        // 1.创建rpc客户端对象
        czrpc::client::rpc_client client;
        try
        {
            // 2.配置连接参数并启动事件循环（非阻塞）
            client.connect({ "127.0.0.1", 50051 }).run();
    
            auto req = std::make_shared<echo_message>();
            req->set_echo_str("Hello world");
            req->set_echo_num(100);
    
            // 3.同步调用echo函数
            auto rsp = client.call("echo", req);
        }
        catch (std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return 0;
        }
    
        std::cin.get();
        return 0;
    }
    ```

    更多例子请查看samples。
    
## 开发平台

* Ubuntu16.04 LTS gcc5.3.1
* MSVC2015

## 依赖性

* boost
* protobuf
* c++14

## DONE

* TCP长连接。
* 同步调用。
* 异步调用。
* worker线程池处理任务。
* 客户端超时处理。
* 支持原始数据发送。
* 支持发布/订阅模式。

## TODO

* 服务注册、发现。
* 支持HTTP/HTTPS协议。


## License
This software is licensed under the [MIT license][3]. © 2017 chxuan


  [1]: http://img.shields.io/badge/license-MIT-blue.svg?style=flat-square
  [2]: https://github.com/chxuan/czrpc/blob/master/LICENSE
