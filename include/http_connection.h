#ifndef _HTTP_CONNECTION_H
#define _HTTP_CONNECTION_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <errno.h>
#include <sys/mman.h>               // mmap
#include <stdarg.h>                 // va_list
#include <stdio.h>
#include <mysql/mysql.h>
#include <functional>
#include <string>
#include <regex>
#include <unordered_map>

#include "buffer.h"
#include "sql_connection_pool.h"
#include "log.h"

#ifdef DEBUG_MODE
#define DEBUG_INFO(x) x
#else
#define DEBUG_INFO(x)
#endif

typedef function<bool(const string&, const string&)> sqlCallback;
typedef function<bool(const string&, const string&, string&)> sqlQueryCallback;

struct ClientInfo {
    struct sockaddr_in address;             // 客户端地址和端口信息
    int sockfd;                             // TCP通信的文件描述符
};

struct HttpInfo {
    string method;
    string url;
    string version;
    long long contentLength;
    bool keepAlive;
    string requestBody;             // 请求体数据
    unordered_map<string, string> headerInfo;
};

class HttpConnection {
private:
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

    enum CONTENT_TYPE {
        TYPE_NULL, TEXT_HTML, TEXT_PLAIN, IMAGE_JPEG, IMAGE_GIF, VIDEO_MPEG
    };

    static constexpr int READ_BUFFER_SIZE = 2048;
    static constexpr int WRITE_BUFFER_SIZE = 1024;

private:
    LINE_STATUS parseLine();
    HTTP_CODE parseRequest();
    HTTP_CODE parseRequestLine(const string& text);
    HTTP_CODE parseRequestHeader(const string& text);
    HTTP_CODE parseRequestBody(const string& text);
    HTTP_CODE doRequest();

    bool writeResponse(HTTP_CODE rqcode);
    bool addResponse(const char* format, ...);
    bool addStatusLine(int status, const char* title);
    bool addHeaders(int contentLength, CONTENT_TYPE contentType = TYPE_NULL);
    bool addContent(const char* content);

public:
    HttpConnection() : readBuffer_(READ_BUFFER_SIZE), writeBuffer_(WRITE_BUFFER_SIZE) {}
    ~HttpConnection() {}

    ssize_t httpRead(int* saveErrno);		// TCP -> readBuffer_
    ssize_t httpWrite(int* saveErrno);		// writeBuffer_ -> TCP
    bool httpProcess();			            // readBuffer_ -> process -> writeBuffer_

    void init(sockaddr_in& address, int sockfd, sqlCallback loginCb, sqlCallback registerCb, sqlQueryCallback sqlQueryCb);
    void init();
    void closeConnection();

    void unmap();
    int toWriteBytes();
    bool isKeepAlive();
    void printClientInfo(bool isConnect);

public:
    static int userCount;                   // 目前已经建立的TCP连接数量
    static bool isEt;                       // 是否为边缘触发模式
    static string rootPath;                 // 服务器资源根目录 
    static regex rqLineRule;
    static regex rqHeaderRule;

private:
    ClientInfo clientInfo_;                 // TCP连接信息
    Buffer readBuffer_;                     // 读缓存 (TCP -> readBuffer_ -> process)
    Buffer writeBuffer_;                    // 写缓存 (process -> writeBuffer_ -> TCP)

    const char* lineEnd_;                   // 一行的结束位置
    CHECK_STATE mainState_;                 
    HttpInfo httpInfo_;                     // 解析得到的结果
    string requestPath_;                    // 请求资源的路径    
    struct stat reqFileState_;              // 请求资源的状态
    char* reqFileAddr_;                     // 客户请求的文件被映射到内存的起始位置
    size_t reqFileSize_;
    bool mmpUsed;

    sqlCallback loginCb_;                   // 数据库查询相关回调
    sqlCallback registerCb_;
    sqlQueryCallback sqlQueryCb_;

    struct iovec iov_[2];
    int iovCount_;  
};

#endif