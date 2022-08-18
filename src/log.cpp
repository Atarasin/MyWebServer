#include "../include/log.h"

static const char* logRootPath = "/home/zh/AtarasinGitRep/MyWebServer/logs";

// 异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const string& logName, int splitLines, int maxQueueSize) {
    // 如果设置了max_queue_size,则设置为异步
    if (maxQueueSize > 0) {
        isAsync_ = true;
        logQueue_ = new BlockQueue<string>(maxQueueSize);
        // 创建线程写日志
        std::thread([this]() {
            threadFlushLog();
        }).detach();
    }

    // 日志的最大行数
    maxSplitLines_ = splitLines;

    // 获取日志文件的路径和文件名
    string::size_type pos = logName.find_last_of('/');
    if (pos == string::npos) {
        logPath_ = string(logRootPath);
        logName_ = logName;
    }
    else {
        logPath_ = logName.substr(0, pos);
        logName_ = logName.substr(pos + 1);
    }
    
    std::time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    struct tm local = *localtime(&now);
    today_ = local.tm_mday;

    stringstream ss;
    ss << put_time(&local, "%Y_%m_%d_");

    logFile_ = logPath_ + "/" + ss.str() + logName_;
    // cout << "logFile: " << logFile_ << endl;

    fp_ = fopen(logFile_.c_str(), "a");
    return (fp_ == nullptr) ? false : true;
}

void Log::writeLog(int level, const char *format, ...) {
    std::time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    struct tm local = *localtime(&now);

    string levelInfo;
    switch (level) {
        case 0:
            levelInfo = "[DEBUG]:";
            break;
        case 1:
            levelInfo = "[INFO]:";
            break;
        case 2:
            levelInfo = "[WARNING]:";
            break;
        case 3:
            levelInfo = "[ERROR]:";
            break;
        default:
            levelInfo = "[INFO]:";
            break;
    }

    {
        lock_guard<mutex> locker(mtx_);
        ++lineCounts_;

        if (today_ != local.tm_mday || lineCounts_ % maxSplitLines_ == 0) {
            fflush(fp_);
            fclose(fp_);

            // 文件名前缀
            stringstream ss;
            ss << put_time(&local, "%Y_%m_%d_");
            logFile_ = logPath_ + "/" + ss.str() + logName_;

            if (today_ != local.tm_mday) {
                today_ = local.tm_mday;
                lineCounts_ = 0;
            }
            // 超过了最大行，在之前的日志名基础上加后缀
            else {
                logFile_ += "." + to_string(lineCounts_ / maxSplitLines_);
            }
            // cout << "newLogFile: " << logFile_ << endl;

            fp_ = fopen(logFile_.c_str(), "a");
        }
    }

    va_list valst;
    // 将传入的format参数赋值给valst，便于格式化输出
    va_start(valst, format);

    string logStr;
    {
        lock_guard<mutex> locker(mtx_);
        // 写入的具体时间内容格式
        char buffer[256] = {'\0'};
        int len = vsnprintf(buffer, sizeof(buffer) - 1, format, valst);
        buffer[len] = '\n';

        stringstream ss;
        ss << put_time(&local, "%Y-%m-%d %H:%M:%S");
        logStr = ss.str() + levelInfo + string(buffer);
    }

    // 若异步, 则将日志信息加入阻塞队列
    if (isAsync_) {
        logQueue_->push(logStr);
    }
    // 若同步, 则直接将数据写入到日志中
    else {
        lock_guard<mutex> locker(mtx_);
        fputs(logStr.c_str(), fp_);
    }

    va_end(valst);
}

void Log::flush(void) {
    lock_guard<mutex> locker(mtx_);
    // 强制刷新写入流缓冲区
    fflush(fp_);
}