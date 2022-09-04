#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "../include/webserver.h"

int main () {
    WebServer server(
        10000,          // 端口号
        5000,           // 执行一次tick的时间
        false,          // 日志模型
        false,          // listenFd ET mode
        "root",         // MySQL用户名
        "root",         // MySQL登录密码
        "webserver"     // MySQL使用的数据库
    );
    DEBUG_INFO(cout << "=====================================" << endl);
    DEBUG_INFO(cout << "webserver starts." << endl);
    DEBUG_INFO(cout << "=====================================" << endl);
    server.start();

    return 0;
}