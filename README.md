# WebServer

本项目是在Linux环境下开发的基于C/C++的轻量级多线程HTTP服务器，支持用户名和密码注册和登录、静态访问服务器的图片、视频等资源。

## 实现功能

- 利用多线程和epoll技术**模拟Proactor模式**，实现服务器高性能并发模型；
- 使用**有限状态机**高效**解析HTTP请求报文**，支持GET和POST请求；
- 实现了**线程池**，并通过设置**监控线程**，可以及时重启运行不正常的线程；
- **基于小顶堆**实现了定时器，可以及时关闭不活跃的TCP连接；
- 实现**同步/异步日志系统**，记录服务器运行状态。

## 测试环境

* Ubuntu18.04
* MySQL5.7.38
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

2. 修改`main.c`中的数据库连接参数，主要包括MySQL服务器的用户名、登录密码以及数据库名。

   ```c++
   connPool->init("localhost", "username", "password", "yourdb", 3306, 8);
   ```

3. 修改`http_conn.cpp`中的服务器资源路径。

   ```c++
   const char* doc_root = ".../MyWebServer/resources";
   ```

修改之后进行编译、运行。

```shell
cd src
make
./server port
```

## 致谢

- Linux高性能服务器编程
- [@qinguoyi](https://github.com/qinguoyi/TinyWebServer)