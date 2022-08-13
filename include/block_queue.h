#ifndef _BLOCK_QUEUE_H
#define _BLOCK_QUEUE_H

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <vector>

using namespace std;

template <class T>
class BlockQueue {
public:
    BlockQueue(int maxSize = INITITAL_QUEUE_CAPACITY_) {
        if (maxSize <= 0) maxSize = INITITAL_QUEUE_CAPACITY_;
        nums_.assign(maxSize + 1, T());

        capacity_ = maxSize + 1;
        front_ = 0;
        rear_ = 0;
    }

    ~BlockQueue() {}

    void clear() {
        lock_guard<mutex> locker(mtx_);
        nums_.clear();
        front_ = 0;
        rear_ = 0;
    }

    bool front(T& value) {
        lock_guard<mutex> locker(mtx_);

        if (empty()) return false;

        value = nums_[front_];
        return true;
    }

    bool back(T& value) {
        lock_guard<mutex> locker(mtx_);

        if (empty()) return false;

        value = nums_[(rear_ - 1 + capacity_) % capacity_];
        return true;
    }

    // producer
    bool push(const T& item) {
        unique_lock<mutex> locker(mtx_);

        if (full()) {
            cond_.notify_all();
            return false;
        }

        nums_[rear_] = item;
        rear_ = (rear_ + 1) % capacity_;
        cond_.notify_all();
        return true;
    }

    // customer
    bool pop(T& item) {
        unique_lock<mutex> locker(mtx_);

        while (empty()) {
            cond_.wait(locker);
        }

        item = nums_[front_];
        front_ = (front_ + 1) % capacity_;
        return true;
    }

private:
    bool full() {
        return front_ == (rear_ + 1) % capacity_ ? true : false;
    }

    bool empty() {
        return front_ == rear_ ? true : false;
    }

private:
    vector<T> nums_;
    int capacity_;
    int front_;
    int rear_;

    mutex mtx_;
    condition_variable cond_;

    static constexpr int INITITAL_QUEUE_CAPACITY_ = 1000;
};

#endif
