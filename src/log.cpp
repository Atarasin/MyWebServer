#include "../include/log.h"

// 异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size)
{
    // 如果设置了max_queue_size,则设置为异步
    if (max_queue_size > 0) {
        m_is_async = true;
        m_log_queue = new BlockQueue<string>(max_queue_size);
        // 创建线程写日志
        std::thread([this]() {
            threadFlushLog();
        }).detach();
    }

    // 日志的缓存区初始化
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    // 日志的最大行数
    m_split_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 从后往前找到第一个/的位置
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    // 相当于自定义日志名
    // 若输入的文件名没有/，则直接将时间+文件名作为日志名
    if (p == NULL) {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else {
        // 截取log文件名
        strcpy(log_name, p + 1);
        // 截取log文件所在的文件夹路径
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");
    if (m_fp == NULL)
    {
        return false;
    }

    return true;
}

void Log::writeLog(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level) {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[erro]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }

    // 写入一个log，对m_count++, m_split_lines最大行数
    {
        lock_guard<mutex> locker(mtx_);
        m_count++;

        // 日志不是今天 或 写入的日志行数是最大行的倍数
        // m_split_lines为最大行数
        if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
        {
            // 新日志的文件名
            char new_log[256] = {0};
            fflush(m_fp);
            fclose(m_fp);
            char tail[16] = {0};
        
            snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        
            // 如果是时间不是今天, 则创建今天的日志, 更新m_today和m_count
            if (m_today != my_tm.tm_mday) {
                snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
                m_today = my_tm.tm_mday;
                m_count = 0;
            }
            else {
                // 超过了最大行，在之前的日志名基础上加后缀, m_count/m_split_lines
                snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
            }
            m_fp = fopen(new_log, "a");
        }
    }

    va_list valst;
    // 将传入的format参数赋值给valst，便于格式化输出
    va_start(valst, format);

    string log_str;
    {
        lock_guard<mutex> locker(mtx_);
        // 写入的具体时间内容格式
        int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                        my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                        my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
        
        int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
        m_buf[n + m] = '\n';
        m_buf[n + m + 1] = '\0';
        log_str = m_buf;
    }

    // 若异步, 则将日志信息加入阻塞队列
    if (m_is_async) {
        m_log_queue->push(log_str);
    }
    // 若同步, 则直接将数据写入到日志中
    else {
        lock_guard<mutex> locker(mtx_);
        fputs(log_str.c_str(), m_fp);
    }

    va_end(valst);
}

void Log::flush(void)
{
    lock_guard<mutex> locker(mtx_);
    // 强制刷新写入流缓冲区
    fflush(m_fp);
}
