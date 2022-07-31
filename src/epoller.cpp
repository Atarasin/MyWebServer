#include "../include/epoller.h"

Epoller::Epoller(int maxEvents) :epollfd_(epoll_create(1)), events_(maxEvents) {
    assert(epollfd_ != -1);
}

Epoller::~Epoller() {
    close(epollfd_);
}

int Epoller::wait(int timeout) {
    return epoll_wait(epollfd_, &events_[0], events_.size(), timeout);
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Epoller::addfd(int fd, bool one_shot) {
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

    epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Epoller::removefd(int fd) {
    epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void Epoller::modfd(int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event);
}

uint32_t Epoller::getEvent(int index) {
    assert(index < events_.size() && index >= 0);
    return events_[index].events;
}

int Epoller::getEventfd(int index) {
    assert(index < events_.size() && index >= 0);
    return events_[index].data.fd;
}
