#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include <sys/types.h> 
#include <sys/socket.h>
#include <errno.h>
#include <string>
#include <netinet/in.h>
#include <functional>

#include "http_conn.h"
#include "timer_minheap.h"
#include "epoller.h"
#include "log.h"
#include "threadpool.h"
#include "sql_connection_pool.h"

using namespace std;

class WebServer {
public:
    WebServer(int port = 10000, int timeout = 5000, bool isAsynLog = false, 
                string sqlUser = "root", string sqlPasswd = "root", 
                string sqlDataBaseName = "webserver");
    ~WebServer();

    void start();

private:
    void InitListenSocket();

    // epoll事件处理函数
    bool dealListenEvent();
    bool dealReadEvent(int sockfd);
    bool dealWriteEvent(int sockfd);
    void closeConnection(int sockfd);
    void closeConnection_(client_data *user_data);

private:
    int listenfd_;                          // 监听的TCP文件描述符
    int port_;                              // 监听端口
    Epoller* epoller_;
    TimerMinHeap* timers_;                  // 定时器最小堆
    ThreadPool<http_conn>* threadPool_;     // 线程池
    connection_pool* sqlConnPool_;          // 数据库连接池

    http_conn* users_;                      // HTTP连接池
    client_data* usersData_;                // 客户端信息集合

    bool isStop_ = false;
    int maxHttpConns_ = 65536;              // 最多的HTTP连接数
};

#endif