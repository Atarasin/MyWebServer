#ifndef _PUBLIC_H
#define _PUBLIC_H

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
#include <string>

#include "log.h"
#include "timer_minheap.h"

//#define connfdET //边缘触发非阻塞
#define connfdLT //水平触发阻塞

//#define listenfdET //边缘触发非阻塞
#define listenfdLT //水平触发阻塞

int setnonblocking(int fd);                             // 对文件描述符设置非阻塞
void printClientInfo(const sockaddr_in& client_address, bool isConnect);  // 打印TCP相关信息 (ip, port)
void show_error(int connfd, const char *info);

//对文件描述符设置非阻塞
inline
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

inline
void printClientInfo(const sockaddr_in& client_address, bool isConnect) {
    char clientIP[16];
    u_int16_t clientPort;

    inet_ntop(AF_INET, &client_address.sin_addr.s_addr, clientIP, sizeof(clientIP));
    clientPort = ntohs(client_address.sin_port);

    if (isConnect) {
        LOG_INFO("A TCP connection is established: ip - %s, port - %d\n", clientIP, clientPort);
        Log::get_instance()->flush();
    }
    else {
        LOG_INFO("A TCP connection is destoried: ip - %s, port - %d\n", clientIP, clientPort);
        Log::get_instance()->flush();
    }
}

inline
void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

#endif