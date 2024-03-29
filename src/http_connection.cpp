#include "../include/http_connection.h"

static const char *ok_200_title = "OK";
static const char *error_400_title = "Bad Request";
static const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
static const char *error_403_title = "Forbidden";
static const char *error_403_form = "You do not have permission to get file form this server.\n";
static const char *error_404_title = "Not Found";
static const char *error_404_form = "The requested file was not found on this server.\n";
static const char *error_500_title = "Internal Error";
static const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 静态成员变量
string HttpConnection::rootPath = "/home/zh99/Projects/MyWebServer/resources/";
bool HttpConnection::isEt = false;
int HttpConnection::userCount = 0;
regex HttpConnection::rqLineRule = regex("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
regex HttpConnection::rqHeaderRule = regex("^([^:]*): ?(.*)$");

void HttpConnection::init(sockaddr_in& address, int sockfd, sqlCallback loginCb, sqlCallback registerCb, sqlQueryCallback sqlQueryCb) {
    clientInfo_.address = address;
    clientInfo_.sockfd = sockfd;
    loginCb_ = loginCb;
    registerCb_ = registerCb;
    sqlQueryCb_ = sqlQueryCb;
    userCount++;
    init();

    printClientInfo(true);
}

void HttpConnection::init() {
    httpInfo_.keepAlive = false;
    httpInfo_.contentLength = 0;
    httpInfo_.method.clear();
    httpInfo_.url.clear();
    httpInfo_.version.clear();

    readBuffer_.reset();
    writeBuffer_.reset();

    mmpUsed = false;
    mainState_ = CHECK_STATE_REQUESTLINE;
}

void HttpConnection::closeConnection() {
    close(clientInfo_.sockfd);
    userCount--;
    printClientInfo(false);
}

void HttpConnection::printClientInfo(bool isConnect) {
    char clientIP[16];
    u_int16_t clientPort;

    inet_ntop(AF_INET, &clientInfo_.address.sin_addr.s_addr, clientIP, sizeof(clientIP));
    clientPort = ntohs(clientInfo_.address.sin_port);

    if (isConnect) {
        LOG_INFO("A TCP connection is established: ip - %s, port - %d", clientIP, clientPort);
        Log::getInstance()->flush();
    }
    else {
        LOG_INFO("A TCP connection is destoried: ip - %s, port - %d", clientIP, clientPort);
        Log::getInstance()->flush();
    }

    LOG_INFO("Current TCP connection num: %d", userCount);
    Log::getInstance()->flush();
}

// TCP -> readBuffer_
ssize_t HttpConnection::httpRead(int* saveErrno) {
    ssize_t len = -1;

    do {
        len = readBuffer_.readFromFd(clientInfo_.sockfd, saveErrno);
        if (len <= 0) break;
    } while (isEt);

    // DEBUG_INFO(readBuffer_.printBuffer());

    return len;
}

// writeBuffer_ -> TCP
ssize_t HttpConnection::httpWrite(int* saveErrno) {
    ssize_t len = -1;

    do {
        len = writev(clientInfo_.sockfd, iov_, iovCount_);
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }
        // 发送完毕
        if (iov_[0].iov_len + iov_[1].iov_len == 0)
            break;
        // iov[0]发送完毕
        else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = static_cast<char*>(iov_[1].iov_base) + (len - iov_[0].iov_len);
            iov_[1].iov_len = iov_[1].iov_len - (len - iov_[0].iov_len);
            if (iov_[0].iov_len > 0) {
                writeBuffer_.readBytesAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = static_cast<char*>(iov_[0].iov_base) + len;
            iov_[0].iov_len = iov_[0].iov_len - len;
            writeBuffer_.readBytes(static_cast<size_t>(len));
        }
    } while (isEt);

    return len;
}

// readBuffer_ -> process -> writeBuffer_
// ture -> 写事件, false -> 读事件
bool HttpConnection::httpProcess() {
    // readBuffer_ -> process
    HTTP_CODE rqcode = parseRequest();
    if (rqcode == NO_REQUEST)
        return false;
    
    // process -> writeBuffer_
    if (!writeResponse(rqcode))
        return false;
    
    // writeBuffer_.printStatus();
    // writeBuffer_.printBuffer();
    
    return true;
}

// readBuffer_ -> process
HttpConnection::HTTP_CODE HttpConnection::parseRequest() {
    if (readBuffer_.readableBytes() <= 0)
        return NO_REQUEST;

    HTTP_CODE rqStatus = NO_REQUEST;
    LINE_STATUS lineStatus = LINE_OK;

    while ((lineStatus = parseLine()) == LINE_OK) {
        string line(readBuffer_.readStartPtrConst(), lineEnd_);

        // DEBUG_INFO(cout << "A new line: " << line << endl);

        switch (mainState_) {
            // 分析请求行
            case CHECK_STATE_REQUESTLINE: {
                rqStatus = parseRequestLine(line);
                if (rqStatus == BAD_REQUEST)
                    return rqStatus;
                break;
            }
            // 分析请求头
            case CHECK_STATE_HEADER: {
                rqStatus = parseRequestHeader(line);
                if (rqStatus == BAD_REQUEST)
                    return rqStatus;
                // GET
                else if (rqStatus == GET_REQUEST)
                    return doRequest();
                break;
            }
            // 分析请求体 (一次全部读取)
            case CHECK_STATE_CONTENT: {
                rqStatus = parseRequestBody(line);
                // POST
                if (rqStatus == GET_REQUEST)
                    return doRequest();
                return NO_REQUEST;          // 这里应该返回, 否则可能会调用readBuffer_.readBytesUntil()出错
            }
            default:
                return INTERNAL_ERROR;
        }

        readBuffer_.readBytesUntil(lineEnd_ + 2);           // 过滤\r\n (若是处于解析请求体的状态, 则不应该执行该语句)
    }

    return NO_REQUEST;
}

HttpConnection::LINE_STATUS HttpConnection::parseLine() {
    // 若是正在解析请求体, buffer中不存在\r\n, 只能根据Content-Length字段确定数据是否完整
    if (mainState_ == CHECK_STATE_CONTENT) {
        if (readBuffer_.readableBytes() < httpInfo_.contentLength)
            return LINE_OPEN;
        
        lineEnd_ = readBuffer_.readStartPtrConst() + httpInfo_.contentLength;
        return LINE_OK;
    }

    // 其它情况根据\r\n判断数据是否完整
    lineEnd_ = readBuffer_.findCRLF();
    return (lineEnd_ == nullptr) ? LINE_OPEN : LINE_OK;
}

/*
    GET /sdsa/sd HTTP/1.1
*/
HttpConnection::HTTP_CODE HttpConnection::parseRequestLine(const string& text) {
    smatch m;
    if (!regex_match(text, m, rqLineRule)) {
        return BAD_REQUEST;
    }
    
    httpInfo_.method = m[1].str();
    httpInfo_.url = m[2].str();
    httpInfo_.version = m[3].str();

    // http://ip:port/xxx | https://ip:port/xxx -> xxx
    string::size_type n = httpInfo_.url.find("//");
    if (n != string::npos)
        httpInfo_.url.erase(0, n + 2);

    n = httpInfo_.url.find("/");
    if (n != string::npos)
        httpInfo_.url.erase(0, n + 1);
    else
        return BAD_REQUEST;

    if (httpInfo_.url.empty())
        httpInfo_.url += "login.html";

    DEBUG_INFO(cout << "httpInfo_.url: " << httpInfo_.url << endl);

    mainState_ = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/*
    Content-length: 1024
    Connection: keep-alive
*/
HttpConnection::HTTP_CODE HttpConnection::parseRequestHeader(const string& text) {
    // 请求行与请求体之间的空行: text = ""
    if (text.empty()) {
        if (httpInfo_.contentLength > 0) {
            mainState_ = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }

    smatch m;

    if (!regex_match(text, m, rqHeaderRule)) {
        return BAD_REQUEST;
    }

    if (m[1].str() == "Content-Length")
        httpInfo_.contentLength = stoll(m[2].str());
    else if (m[1].str() == "Connection" && m[2].str() == "keep-alive")
        httpInfo_.keepAlive = true;

    httpInfo_.headerInfo[m[1].str()] = m[2].str();

    return NO_REQUEST;
}

/*
    user=123&passwd=123
*/
HttpConnection::HTTP_CODE HttpConnection::parseRequestBody(const string& text) {
    // 判断数据是否全部被读入
    if (readBuffer_.readableBytes() < httpInfo_.contentLength)
        return NO_REQUEST;
    
    httpInfo_.requestBody = text.substr(0, httpInfo_.contentLength);
    return GET_REQUEST;
}

// 处理URL和请求体数据
HttpConnection::HTTP_CODE HttpConnection::doRequest() {
    requestPath_ = rootPath;

    // xCGISQL.cgi (x = 2 | 3)
    if (httpInfo_.method == "POST" && (httpInfo_.url[0] == '2' || httpInfo_.url[0] == '3')) {
        // user=123&passwd=123
        string::size_type pos = httpInfo_.requestBody.find("&");
        string name = httpInfo_.requestBody.substr(5, pos - 5);
        string passwd = httpInfo_.requestBody.substr(pos + 8);

        DEBUG_INFO(cout << "body: " << httpInfo_.requestBody << endl);
        DEBUG_INFO(cout << "name:" << name << "," << "passwd:" << passwd << endl);

        // 注册
        if (httpInfo_.url[0] == '3') {
            if (registerCb_(name, passwd)) httpInfo_.url = "login.html";
            else httpInfo_.url = "registerError.html";
        }

        // 登录
        else if (httpInfo_.url[0] == '2') {
            if (loginCb_(name, passwd)) httpInfo_.url = "welcome.html";
            else httpInfo_.url = "loginError.html";
        }
    }

    // 访问数据库并将访问结果写入到内存中
    if (httpInfo_.method == "POST" && httpInfo_.url == "sql_query") {
        // database=mydatabase&table=orders
        string::size_type pos = httpInfo_.requestBody.find("&");
        string dbName = httpInfo_.requestBody.substr(9, pos - 9);
        string tbName = httpInfo_.requestBody.substr(pos + 7);

        DEBUG_INFO(cout << "body: " << httpInfo_.requestBody << endl);
        DEBUG_INFO(cout << "database: " << dbName << "," << "table: " << tbName << endl);

        string sqlResult;
        sqlResult += "<html><body><br/>";

        if (sqlQueryCb_(dbName, tbName, sqlResult)) {
            sqlResult += "</body></html>";
            DEBUG_INFO(cout << "sql success." << endl);
            // DEBUG_INFO(cout << "sqlResult: " << sqlResult << endl);
            
            mmpUsed = false;
            reqFileAddr_ = const_cast<char*>(sqlResult.c_str());
            reqFileSize_ = sqlResult.size() + 1;    // '\0'

            return FILE_REQUEST;
        }
        else  httpInfo_.url = "otherError.html";
    }

    if (httpInfo_.url[0] == '5')
        requestPath_ += "picture.html";

    else if (httpInfo_.url[0] == '6')
        requestPath_ += "video.html";

    else if (httpInfo_.url[0] == '7') 
        requestPath_ += "other.html";

    else requestPath_ += httpInfo_.url;

    // 获得客户端请求资源文件的权限
    if (stat(requestPath_.c_str(), &reqFileState_) < 0) {
        // perror("stat");
        return NO_RESOURCE;
    }
    // 判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if (!(reqFileState_.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    // 判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if (S_ISDIR(reqFileState_.st_mode))
        return BAD_REQUEST;

    // 打开资源文件
    int fd = open(requestPath_.c_str(), O_RDONLY);
    // 将资源文件映射到内存 MAP_PRIVATE
    reqFileAddr_ = (char *)mmap(0, reqFileState_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    reqFileSize_ = reqFileState_.st_size;
    mmpUsed = true;
    close(fd);

    return FILE_REQUEST;
}

// process -> writeBuffer_
bool HttpConnection::writeResponse(HTTP_CODE rqcode) {
    if (writeBuffer_.writableBytes() <= 0)
        return false;

    switch (rqcode) {
        case INTERNAL_ERROR: {
            addStatusLine(500, error_500_title);
            addHeaders(strlen(error_500_form));
            if (!addContent(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST: {
            addStatusLine(404, error_404_title);
            addHeaders(strlen(error_404_form));
            if (!addContent(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST: {
            addStatusLine(403, error_403_title);
            addHeaders(strlen(error_403_form));
            if (!addContent(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST: {
            addStatusLine(200, ok_200_title);
            if (reqFileSize_ != 0) {
                mmpUsed ? addHeaders(reqFileSize_) : addHeaders(reqFileSize_, TEXT_HTML);
                iov_[0].iov_base = writeBuffer_.readStartPtr();
                iov_[0].iov_len = writeBuffer_.readableBytes();
                iov_[1].iov_base = reqFileAddr_;
                iov_[1].iov_len = reqFileSize_;
                iovCount_ = 2;
                return true;
            }
            else {
                const char *ok_string = "<html><body></body></html>";
                addHeaders(strlen(ok_string));
                if (!addContent(ok_string))
                    return false;
            }
        }
        default:
            return false;
    }

    iov_[0].iov_base = writeBuffer_.readStartPtr();
    iov_[0].iov_len = writeBuffer_.readableBytes();
    iovCount_ = 1;
    
    return true;
}

bool HttpConnection::addResponse(const char* format, ...) {
    va_list argsList;
    va_start(argsList, format);

    char buffer[256] = {0};
    int len = vsnprintf(buffer, sizeof(buffer), format, argsList);
    if (len < 0) {
        va_end(argsList);
        return false;
    }

    va_end(argsList);
    writeBuffer_.writeBytes(buffer, static_cast<size_t>(len));

    return true;
}

bool HttpConnection::addStatusLine(int status, const char* title) {
    return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConnection::addHeaders(int contentLength, CONTENT_TYPE contentType) {
    switch (contentType) {
        case TYPE_NULL: break;
        case TEXT_HTML: 
            if (!addResponse("Content-Type: %s\r\n", "text/html;charset=utf-8")) return false;
            break;
        case TEXT_PLAIN: 
            if (!addResponse("Content-Type: %s\r\n", "text/plain;charset=utf-8")) return false;
            break;
        default: break;
    }

    if (!addResponse("Content-Length: %d\r\n", contentLength))
        return false;
    if (!addResponse("Connection: %s\r\n", (httpInfo_.keepAlive == true) ? "keep-alive" : "close"))
        return false;
    if (!addResponse("\r\n"))
        return false;
    return true;
}

bool HttpConnection::addContent(const char* content) {
    if (!addResponse("%s", content))
        return false;
    return true;
}

void HttpConnection::unmap() {
    if (mmpUsed && reqFileAddr_) {
        munmap(reqFileAddr_, reqFileState_.st_size);
        reqFileAddr_ = nullptr;
        mmpUsed = false;
    }
}

int HttpConnection::toWriteBytes() { 
    return iov_[0].iov_len + iov_[1].iov_len; 
}

bool HttpConnection::isKeepAlive() {
    return httpInfo_.keepAlive;
}