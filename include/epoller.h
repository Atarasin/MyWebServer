#ifndef _EPOLLER_H
#define _EPOLLER_H

#include <sys/epoll.h>
#include <vector>

#include "public.h"

class Epoller {
public:
    Epoller(int maxEvents = 10000);
    ~Epoller();

    int wait(int timeout);
    uint32_t getEvent(int index);
    int getEventfd(int index);

    void addfd(int fd, bool one_shot);          // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void removefd(int fd);                      // 从内核时间表删除描述符
    void modfd(int fd, int ev);                 // 将事件重置为EPOLLONESHOT

private:
    int epollfd_;           
    vector<struct epoll_event> events_;                    // 内核事件表
};

#endif