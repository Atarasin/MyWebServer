#ifndef _HTTP_CONNECTION_H
#define _HTTP_CONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <mysql/mysql.h>
#include <fstream>

#include "locker.h"
#include "sql_connection_pool.h"
#include "public.h"
#include "log.h"
#include "epoller.h"

class http_conn {
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

    enum METHOD {
        GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH
    };
    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:    当前正在分析请求行
        CHECK_STATE_HEADER:         当前正在分析头部字段
        CHECK_STATE_CONTENT:        当前正在解析请求体
    */
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT
    };
    /*
        从状态机的三种可能状态: 
        LINE_OK:                    读到一个完整行
        LINE_BAD:                   行出错
        LINE_OPEN:                  行数据尚且不完整
    */
    enum LINE_STATUS {
        LINE_OK = 0, LINE_BAD, LINE_OPEN
    };
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整, 需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求, 获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE {
        NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, Epoller* epoller);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();   // 将客户端的HTTP请求读取到读缓存中
    bool write();       // 将HTTP响应从写缓存发送给客户端
    sockaddr_in *get_address()
    {
        return &m_address;
    }

    void initmysql_result(connection_pool *connPool);

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);

    // 解析HTTP请求
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    // 生成HTTP响应正文, 即将本地资源映射到内存中
    HTTP_CODE do_request();

    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();
    void unmap();

    // 生成HTTP响应
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    // static int m_epollfd;
    static int m_user_count;    // 目前已经建立的TCP连接数量
    MYSQL *mysql;

private:
    int m_sockfd;
    Epoller* m_epoller;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];      // 读缓存
    int m_read_idx;                         // 读缓存下一个应该存放的数据索引
    int m_checked_idx;                      // 正在进行HTTP解析的字符在读缓存中的索引
    int m_start_line;                       // 正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE];    // 写缓存
    int m_write_idx;                        // 写缓存中待发送的字节数
    CHECK_STATE m_check_state;              // 主状态机目前所处的状态
    METHOD m_method;
    char m_real_file[FILENAME_LEN];         // 客户请求的目标文件的完整路径(即访问的网页资源)
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;
    char *m_file_address;                   // 客户请求的文件被映射到内存的起始位置
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;                                // 是否启用的POST
    char *m_string;                         // 存储请求头数据
    int bytes_to_send;                      // 将要发送数据的字节数
    int bytes_have_send;                    // 已经发送数据的字节数
};

#endif
