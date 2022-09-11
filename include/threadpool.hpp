#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>
#include <functional>
#include <atomic>

template <typename Task>
class ThreadPool {
private:
    int threadNum_;                             // 线程最大数量
    int maxRequestNum_;                         // 请求队列的最大容量
    std::queue<Task> requests_;                 // 请求队列
    std::mutex mtx_;                            // 互斥锁
    std::condition_variable cond_;              // 条件变量
    atomic<bool> isStop_;                       // 线程停止标志

public:
    ThreadPool(int threadNum = 8, int maxRequestNum = 10000) : 
                threadNum_(threadNum), maxRequestNum_(maxRequestNum), isStop_(false) {
        assert(threadNum_ >0);
        assert(maxRequestNum_ > 0);

        for (int i = 0; i < threadNum_; i++) {
            std::thread([this]() {
                doTask();
            }).detach();
        }
    }

    ~ThreadPool() {
        isStop_.store(true);
    }

    bool append(Task&& task) {
        {
            std::unique_lock<std::mutex> locker(mtx_);
            if (requests_.size() >= maxRequestNum_)
                return false;

            requests_.push(task);
        }
        cond_.notify_one();

        return true;
    }

private:
    void doTask() {
        while (!isStop_.load()) {
            Task request = nullptr;
            {
                std::unique_lock<std::mutex> locker(mtx_);

                while (requests_.empty()) {
                    cond_.wait(locker);
                }

                request = requests_.front();
                requests_.pop();
            }

            if (request == nullptr || isStop_.load())
                continue;
            
            request();
        }
    }
};

#endif