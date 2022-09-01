#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "block_queue.h"
#include "buffer.h"

using namespace std;

class Log {
public:
    static Log *getInstance() {
        static Log instance;
        return &instance;
    }

    // customer
    void threadFlushLog() {
        cout << "write log starts." << endl;
        Log::getInstance()->asyncWriteLog();
    }

    // 可选择的参数有日志文件、最大行数以及最长日志条队列
    bool init(const string& logName, int splitLines = 50000, int maxQueueSize = 0);

    // 将输出内容按照标准格式整理
    void writeLog(int level, const char *format, ...);     // producer

    // 强制刷新缓冲区
    void flush(void);

private:
    Log() {
        lineCounts_ = 0;
        isAsync_ = false;
    }

    virtual ~Log() {
        if (fp_ != NULL) fclose(fp_);
    }

    // 异步写日志方法
    void *asyncWriteLog() {
        string logStr;
        // 从阻塞队列中取出一个日志string，写入文件
        while (logQueue_->pop(logStr)) {
            {
                lock_guard<mutex> locker(mtx_);
                fputs(logStr.c_str(), fp_);
            }
        }
    }

private:
    string logPath_;                        // 日志路径
    string logName_;                        // 日志文件名
    string logFile_;                        // 完整路径

    int maxSplitLines_;                     // 日志最大行数
    long long lineCounts_;                  // 日志行数记录
    int today_;                             // 因为按天分类, 记录当前时间是那一天
    FILE *fp_;                              // 打开log的文件指针

    BlockQueue<string> *logQueue_;           // 阻塞队列
    bool isAsync_;                           // 是否同步标志位
    mutex mtx_;
};


// 这四个宏定义在其他文件中使用，主要用于不同类型的日志输出
#define LOG_DEBUG(format, ...) Log::getInstance()->writeLog(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::getInstance()->writeLog(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::getInstance()->writeLog(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::getInstance()->writeLog(3, format, ##__VA_ARGS__)

#endif