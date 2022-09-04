#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include <sys/types.h> 
#include <sys/socket.h>
#include <errno.h>
#include <string>
#include <netinet/in.h>
#include <functional>
#include <map>
#include <mutex>
#include <mysql/mysql.h>

#include "http_connection.h"
#include "timer_minheap.h"
#include "epoller.h"
#include "log.h"
#include "threadpool.h"
#include "sql_connection_pool.h"

using namespace std;

typedef function<bool(const string&, const string&)> sqlCallback;
typedef function<void()> taskCallback;

class WebServer {
public:
    WebServer(int port = 10000, int timeout = 5000, bool isAsynLog = false, bool isET = false,
                string sqlUser = "root", string sqlPasswd = "root", 
                string sqlDataBaseName = "webserver");
    ~WebServer();

    void start();

private:
    void initListenSocket();
    void initUserCache();

    bool loginVerify(const string& userName, const string& passwd);
    bool registerVerify(const string& userName, const string& passwd);
    void updateUserCache(const string& userName, const string& passwd);
    bool sqlQuery(const char* sqlOrder, string& sqlResult);

    // epoll事件处理函数
    bool dealListenEvent();
    bool dealReadEvent(int sockfd);
    bool dealWriteEvent(int sockfd);
    void closeConnection(int sockfd);
    void closeConnection_(int sockfd);

    void process(int sockfd);

private:
    int listenfd_;                          // 监听的TCP文件描述符
    int port_;                              // 监听端口
    Epoller* epoller_;
    TimerMinHeap* timers_;                  // 定时器最小堆
    ThreadPool<taskCallback>* threadPool_;  // 线程池
    connection_pool* sqlConnPool_;          // 数据库连接池

    HttpConnection* users_;                 // HTTP连接
    client_data* usersData_;                // 客户端信息集合

    map<string, string> userCache_;          // 记录用户名和密码
    mutex userCacheMtx_;
    bool isET_;

    bool isStop_ = false;
    int maxHttpConns_ = 65536;              // 最多的HTTP连接数
};

#endif