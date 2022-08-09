#include "../include/timer_minheap.h"

// 往上调整
void TimerMinHeap::siftup(int index) {
    while (index > 0) {
        int parentIdx = (index - 1) / 2;
        if (minheap[parentIdx]->expire <= minheap[index]->expire)
            break;
        
        swap(minheap[parentIdx], minheap[index]);
        timerToIdx[minheap[index]] = index;

        index = parentIdx;
    }

    timerToIdx[minheap[index]] = index;
}

// 往下调整
void TimerMinHeap::siftdown(int index) {
    int minIdx;
    while (index < minheap.size()) {
        minIdx = index;
        int leftIdx = index * 2 + 1;
        int rightIdx = index * 2 + 2;

        if (leftIdx < minheap.size() && minheap[leftIdx]->expire < minheap[minIdx]->expire)
            minIdx = leftIdx;
        if (rightIdx < minheap.size() && minheap[rightIdx]->expire < minheap[minIdx]->expire)
            minIdx = rightIdx;
        
        if (minIdx == index) break;

        swap(minheap[index], minheap[minIdx]);
        timerToIdx[minheap[index]] = index;

        index = minIdx;
    }

    timerToIdx[minheap[index]] = index;
}

// 添加新的定时器
void TimerMinHeap::add_timer(util_timer* timer, timeoutCBFunct cb) {
    if (timer == nullptr)
        return;

    timer->cbFunc = cb;
    
    minheap.push_back(timer);
    siftup(minheap.size() - 1);
}

// 调整定时器的位置
void TimerMinHeap::adjust_timer(util_timer* timer) {
    // 若没找到, 则不处理
    if (timer == nullptr || timerToIdx.find(timer) == timerToIdx.end())
        return;
    
    int index = timerToIdx[timer];
    siftdown(index);
}


// 删除指定的定时器
void TimerMinHeap::del_timer(util_timer* timer) {
    if (timer == nullptr || timerToIdx.find(timer) == timerToIdx.end())
        return;
    
    int index = timerToIdx[timer];

    // 将被删除的定时器与最后一个定时器交换, 删除最后一个定时器
    swap(minheap[index], minheap.back());
    timerToIdx.erase(timerToIdx.find(timer));
    minheap.pop_back();

    delete timer;

    // 调整被交换的定时器
    siftdown(index);
}

// 删除所有超时的定时器
void TimerMinHeap::tick() {
    if (minheap.empty()) 
        return;

    time_t curtime = time(NULL);

    while (!minheap.empty() && minheap.front()->expire < curtime) {
        // 释放当前连接
        minheap.front()->cbFunc();
        del_timer(minheap.front());
    }
}

int TimerMinHeap::doTick() {
    tick();
    return delayMS;
}

int TimerMinHeap::size() {
    return minheap.size();
}