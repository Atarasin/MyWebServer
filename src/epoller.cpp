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
void Epoller::addfd(int fd, bool isET, bool oneshot, bool nonblock) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    if (isET) event.events |= EPOLLET;
    if (oneshot) event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event);

    if (nonblock) setNonBlocking(fd);
}

void Epoller::removefd(int fd) {
    epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void Epoller::modfd(int fd, uint32_t ev, bool isET, bool oneshot) {
    epoll_event event;                  // 每次都是重新设置一个新的事件结构体
    event.data.fd = fd;

    event.events = ev | EPOLLRDHUP; 

    if (isET) event.events |= EPOLLET;
    if (oneshot) event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event);
}

int Epoller::setNonBlocking(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

uint32_t Epoller::getEvent(int index) {
    assert(index < events_.size() && index >= 0);
    return events_[index].events;
}

int Epoller::getEventfd(int index) {
    assert(index < events_.size() && index >= 0);
    return events_[index].data.fd;
}
