# WebServer

本项目是在Linux环境下开发的基于C/C++的轻量级多线程HTTP服务器，支持远程数据库访问、用户密码注册和登录、静态访问服务器的图片、视频等资源。

## 实现功能

- 利用多线程和epoll技术**模拟Proactor模式**，实现服务器高性能并发模型；
- 使用**有限状态机**高效**解析HTTP请求报文**，支持GET和POST请求；
- 基于function设计了**线程池模板类**，可以通过添加不同的请求而执行不同的任务函数；
- **基于小顶堆**实现了定时器，可以及时关闭不活跃的TCP连接；
- 实现**同步/异步日志系统**，记录服务器运行状态；
- 基于vector设计了**可自动增长的缓存区**，可以适应不同大小的报文数据；
- 基于LFU技术实现了**数据库缓存**功能，可以降低数据库的访问量。

## 测试环境

* Ubuntu 18.04
* MySQL 5.7.38
* C++ 11

## 项目运行

在使用前需要进行如下修改：

1. 在MySQL中创建用于存放用户名和密码的数据库表。

   ```mysql
   -- 建立yourdb库
   create database yourdb;
   
   -- 创建user表
   USE yourdb;
   CREATE TABLE user(
       username char(50) NULL,
       passwd char(50) NULL
   )ENGINE=InnoDB;
   
   -- 添加数据
   INSERT INTO user(username, passwd) VALUES('name', 'passwd');
   ```

2. 修改`main.cpp`中的服务器的端口号、数据库连接参数(包括MySQL服务器的用户名、登录密码以及数据库名)。

   ```c++
   WebServer server(
        10000,              // 端口号
        5000,               // 执行一次tick的时间
        false,              // 日志模型
        false,              // listenFd ET mode
        "username",         // MySQL用户名
        "passward",         // MySQL登录密码
        "dbname"            // MySQL使用的数据库
    );
   ```

3. 修改`http_connection.cpp`中的服务器资源路径。

   ```c++
   string HttpConnection::rootPath = ".../MyWebServer/resources/";
   ```

4. 修改`log.cpp`中的默认日志存放路径。

   ```c++
   static const char* logRootPath = ".../MyWebServer/logs";;
   ```

修改之后进行编译、运行。

```shell
mkdir build && cd build
cmake ..
make
./server
```

## 致谢

- Linux高性能服务器编程
- Linux多线程服务端编程：使用muduo C++网络库
- [@qinguoyi](https://github.com/qinguoyi/TinyWebServer)
- [@markparticle](https://github.com/markparticle/WebServer)