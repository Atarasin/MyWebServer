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

#include "../include/locker.h"
#include "../include/timer_minheap.h"
#include "../include/http_conn.h"
#include "../include/log.h"
#include "../include/sql_connection_pool.h"
#include "../include/threadpool.h"
#include "../include/webserver.h"
#include "../include/public.h"

int main () {
    WebServer server(
        10000,          // 端口号
        5000,           // 执行一次tick的时间
        false,          // 日志模型
        "root",         // MySQL用户名
        "root",         // MySQL登录密码
        "webserver"     // MySQL使用的数据库
    );
    server.start();

    return 0;
}