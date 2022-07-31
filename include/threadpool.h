#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <list>
#include <vector>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include "locker.h"
#include "sql_connection_pool.h"
#include "log.h"

// 线程信息结构体
struct pthread_info {
    pthread_t pid;      // 线程id
    time_t    atime;    // 最近一次的活动时间
};

template <typename T>
class ThreadPool {
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    ThreadPool(connection_pool *connPool, int thread_number = 10, int max_requests = 10000);
    ~ThreadPool();
    bool append(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    static void *checkpool(void *arg);
    void run();

private:
    int m_thread_number;                    // 线程池中的线程数
    int m_max_requests;                     // 请求队列中允许的最大请求数
    std::vector<pthread_info> m_threads;    // 描述线程池的数组，其大小为m_thread_number
    pthread_info m_checkpool;               // 线程池监控线程
    std::list<T*> m_workqueue;              // 请求队列
    locker m_queuelocker;                   // 保护请求队列的互斥锁
    sem m_queuestat;                        // 是否有任务需要处理
    bool m_stop;                            // 是否结束线程
    connection_pool *m_connPool;            // 数据库
};

// 创建多个线程并设置线程分离
template <typename T>
ThreadPool<T>::ThreadPool(connection_pool *connPool, int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_connPool(connPool) {
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();

    for (int i = 0; i < thread_number; ++i) {
        //printf("create the %dth thread\n",i);
        pthread_info m_thread;
        // 创建线程
        if (pthread_create(&m_thread.pid, NULL, worker, this) != 0) {
            // delete[] m_threads;
            throw std::exception();
        }
        // 设置线程分离
        if (pthread_detach(m_thread.pid)) {
            // delete[] m_threads;
            throw std::exception();
        }

        m_thread.atime = time(0);
        m_threads.push_back(m_thread);
    }

    // 创建监控线程, 用于监控线程池中的所有线程
    if (pthread_create(&m_checkpool.pid, NULL, checkpool, this) != 0) {
        throw std::exception();
    }
    // 设置线程分离
    if (pthread_detach(m_checkpool.pid)) {
        throw std::exception();
    }
    m_checkpool.atime = time(0);

    printf("线程池初始化结束\n");
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
    // delete[] m_threads;
    m_stop = true;
}


template <typename T>
bool ThreadPool<T>::append(T *request)
{
    m_queuelocker.lock();
    // 若请求队列长度已经超过最大请求数, 则抛弃这个请求
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    // 每加入一个请求, 信号量m_queuestat就+1
    m_queuestat.post();

    return true;
}

// 传入参数为this指针
template <typename T>
void *ThreadPool<T>::worker(void *arg)
{
    usleep(1000);
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();
    return pool;
}

// 线程监控函数
template <typename T>
void *ThreadPool<T>::checkpool(void *arg)
{
    printf("start checkpool ...\n");

    ThreadPool *pool = (ThreadPool *)arg;

    while (true) {
        // 遍历所有的工作线程, 查看是否超时
        for (int i = 0; i < pool->m_threads.size(); i++) {
            // 工作线程超时时间为10s, 这里用15秒判断超时
            if ((time(0) - pool->m_threads[i].atime) > 15) {
                LOG_INFO("thread %ld time out.", pool->m_threads[i].pid);
                Log::get_instance()->flush();

                // 取消超时的工作线程
                pthread_cancel(pool->m_threads[i].pid);

                // 重新创建一个工作线程
                if (pthread_create(&pool->m_threads[i].pid, NULL, worker, pool) != 0) {
                    // delete[] m_threads;
                    throw std::exception();
                }
                // 设置线程分离
                if (pthread_detach(pool->m_threads[i].pid)) {
                    // delete[] m_threads;
                    throw std::exception();
                }
                pool->m_threads[i].atime = time(0);
            }
        }

        sleep(5);
    }

    printf("end checkpool ...\n");
}

// 1. 当线程池初始化时, run()就已经开始运行了
// 2. 但是信号量m_queuestat阻塞了run()
// 3. 只有当添加请求时, 信号量m_queuestat + 1, 当信号量m_queuestat>0时, 才开始真正执行请求程序
template <typename T>
void ThreadPool<T>::run()
{
    // 获得线程编号
    // 线程id可能会被checkpool线程改变, 访问时应该加锁
    int pthread_num = -1;
    for (int i = 0; i < m_threads.size(); i++) {
        if (pthread_equal(pthread_self(), m_threads[i].pid) != 0) {
            pthread_num = i;
            break;
        }
    }
    if (pthread_num == -1) {
        printf("没有找到线程索引\n");
        throw std::exception();
    }
    LOG_INFO("pthread-%ld(index-%d) starts to run.", m_threads[pthread_num].pid, pthread_num);
    Log::get_instance()->flush();

    while (!m_stop)
    {
        // 当信号量m_queuestat==0时, 阻塞
        // m_queuestat.wait();
        bool notwait = false;
        while (!notwait) {
            struct timeval now;
            gettimeofday(&now, NULL);
            now.tv_sec = now.tv_sec + 10;
            notwait = m_queuestat.timedwait((struct timespec*)&now);
            // if (pthread_num != 1)
            m_threads[pthread_num].atime = time(0);
        }

        // ----------------------- lock -----------------------
        m_queuelocker.lock();

        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        // 从请求队列中取出一个请求
        T *request = m_workqueue.front();
        m_workqueue.pop_front();

        m_queuelocker.unlock();
        // ---------------------- unlock ----------------------

        if (!request)
            continue;

        // 将数据库连接传入到request->mysql
        connectionRAII mysqlcon(&request->mysql, m_connPool);
        
        // 执行请求
        request->process();
    }
}

#endif
