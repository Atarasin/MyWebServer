#ifndef _TIMER_MINHEAP_H
#define _TIMER_MINHEAP_H

#include <time.h>
#include <vector>
#include <unordered_map>
#include "log.h"
#include "public.h"

class util_timer;           // 前向引用声明

struct client_data {
    sockaddr_in address;    // 客户端地址和端口信息
    int sockfd;             // TCP通信的文件描述符
    util_timer *timer;      // 由于两者相互引用, 只能使用指针表示, 否则无法通过编译
};

// 定时器节点
class util_timer {
public:
    time_t expire;                      // 定时器释放时间, 可以释放不活跃的TCP连接
    void (*cb_func)(client_data *);     // 定时器回调函数
    client_data *user_data;

public:
    util_timer() {}
};

// 最小堆实现定时器排序
class TimerMinHeap {
private:
    vector<util_timer*> minheap;
    unordered_map<util_timer*,int> timerToIdx;

private:
    void siftup(int index);
    void siftdown(int index);

public:
    TimerMinHeap() {}
    ~TimerMinHeap() {}

    // 在数组末端添加定时器节点, 然后往上调整
    void add_timer(util_timer* timer);
    // expire只会增加, 故应该向下调整
    void adjust_timer(util_timer* timer);
    // 删除指定的定时器
    void del_timer(util_timer* timer);
    // 移除已到释放时间的所有定时器
    void tick();
    int size();
};

#endif