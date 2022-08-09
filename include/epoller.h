#ifndef _EPOLLER_H
#define _EPOLLER_H

#include <sys/epoll.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <vector>

using namespace std;

class Epoller {
public:
    Epoller(int maxEvents = 10000);
    ~Epoller();

    int wait(int timeout);
    uint32_t getEvent(int index);
    int getEventfd(int index);

    void addfd(int fd, bool isET, bool oneshot, bool nonblock);    
    void removefd(int fd);
    void modfd(int fd, uint32_t ev, bool isET, bool oneshot);          
    int setNonBlocking(int fd);              

private:
    int epollfd_;           
    vector<struct epoll_event> events_;                    // 内核事件表
};

#endif