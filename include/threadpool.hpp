#ifndef _THREADPOOL_HPP
#define _THREADPOOL_HPP

#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>
#include <functional>
#include <atomic>

class ThreadPool {
private:
    typedef std::function<void()>   Task;
    int maxRequestNum_;                         // 请求队列的最大容量
    std::vector<std::thread> workers_;          // 存放所有线程实例
    std::queue<Task> requests_;                 // 请求队列
    std::mutex mtx_;                            // 互斥锁
    std::condition_variable cond_;              // 条件变量
    atomic<bool> isStop_;                       // 线程停止标志

private:
    void doTask() {
        while (!isStop_) {
            Task request;
            {
                std::unique_lock<std::mutex> locker(mtx_);
                cond_.wait(locker, [this]{ return isStop_ || !requests_.empty(); });

                if (isStop_ || requests_.empty()) return;

                request = std::move(requests_.front());
                requests_.pop();
            }
            request();
        }
    }

public:
    ThreadPool(int threadNum = 8, int maxRequestNum = 10000) : 
                maxRequestNum_(maxRequestNum), isStop_(false) {
        for (int i = 0; i < threadNum; i++) {
            workers_.push_back(std::thread(&ThreadPool::doTask, this));
        }
    }

    ~ThreadPool() {
        isStop_ = true;
        cond_.notify_all();

        for (std::thread& worker : workers_) {
            worker.join();
        }
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
};

#endif