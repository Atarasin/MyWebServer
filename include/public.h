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

//#define connfdET //边缘触发非阻塞
#define connfdLT //水平触发阻塞

//#define listenfdET //边缘触发非阻塞
#define listenfdLT //水平触发阻塞

#define SYNLOG  //同步写日志
//#define ASYNLOG //异步写日志

int setnonblocking(int fd);                             // 对文件描述符设置非阻塞
void addfd(int epollfd, int fd, bool one_shot);         // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void removefd(int epollfd, int fd);                     // 从内核时间表删除描述符
void modfd(int epollfd, int fd, int ev);                // 将事件重置为EPOLLONESHOT
void printClientInfo(const sockaddr_in& client_address, bool isConnect);  // 打印TCP相关信息 (ip, port)

//对文件描述符设置非阻塞
inline
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
inline
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef listenfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//从内核时间表删除描述符
inline
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
inline
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
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

#endif