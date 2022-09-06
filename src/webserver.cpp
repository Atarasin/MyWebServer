#include "../include/webserver.h"

WebServer::WebServer(int port, int timeout, bool isAsynLog, bool isET, string sqlUser, string sqlPasswd, string sqlDataBaseName) : 
    port_(port), isET_(isET), epoller_(new Epoller()), timers_(new TimerMinHeap(timeout)),
    threadPool_(new ThreadPool<taskCallback>()),
    users_(new HttpConnection[maxHttpConns_]),
    usersData_(new client_data[maxHttpConns_]) {

    isStop_ = false;

    // 初始化监听描述符
    initListenSocket();

    // 初始化数据库连接池
    sqlConnPool_ = SQLConnectionPool::GetInstance();
    sqlConnPool_->init("localhost", sqlUser, sqlPasswd, sqlDataBaseName, 3306, 8);
    
    // 初始化用户信息(用户名和密码)
    initUserCache();

    // 初始化日志
    if (isAsynLog)
        Log::getInstance()->init("ServerLog", 800000, 8);  // 异步日志模型
    else
        Log::getInstance()->init("ServerLog", 800000, 0);  // 同步日志模型
}

WebServer::~WebServer() {
    close(listenfd_);
}

void WebServer::initListenSocket() {
    listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd_ >= 0);

    // 监听listenfd事件
    epoller_->addfd(listenfd_, isET_, false, true);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    int flag = 1; int ret = 0;
    // 设置端口复用
    setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd_, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd_, 5);
    assert(ret >= 0);
}

void WebServer::initUserCache() {
    // 先从连接池中取一个连接
    MYSQL* mysql = nullptr;
    SQLConnectionRAII mysqlcon(&mysql, sqlConnPool_);

    // 在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user")) {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD* fields = mysql_fetch_fields(result);

    // 从结果集中获取下一行, 将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        userCache_[string(row[0])] = string(row[1]);
    }

    for (auto it = userCache_.begin(); it != userCache_.end(); ++it)
        DEBUG_INFO(cout << "user: " << it->first << ", " << "passwd: " << it->second << endl);
}

bool WebServer::loginVerify(const string& userName, const string& passwd) {
    // 先查看userCache中是否存在
    if (userCache_.find(userName) != userCache_.end()) {
        DEBUG_INFO(cout << "in cache" << endl);
        return userCache_[userName] == passwd ? true : false;
    }

    DEBUG_INFO(cout << "in mysql server" << endl);

    // 若不存在, 则查询数据库
    MYSQL* mysql = nullptr;
    SQLConnectionRAII mysqlcon(&mysql, sqlConnPool_);
    
    char order[256] = {0};
    snprintf(order, 256, "SELECT username,passwd FROM user WHERE username='%s' LIMIT 1", userName.c_str());

    MYSQL_FIELD* fields = nullptr;
    MYSQL_RES* res = nullptr;

    if (mysql_query(mysql, order)) {
        mysql_free_result(res);
        return false;
    }

    res = mysql_store_result(mysql);
    fields = mysql_fetch_fields(res);

    MYSQL_ROW row = mysql_fetch_row(res);
    
    // 用户名不存在或用户名对应的密码错误
    if (row == NULL || passwd != string(row[1]))
        return false;
    
    mysql_free_result(res);
    
    updateUserCache(userName, passwd);   // 更新缓存
    
    return true;
}

bool WebServer::registerVerify(const string& userName, const string& passwd) {
    // 若用户名已经存在则直接返回false
    if (userCache_.find(userName) != userCache_.end()) {
        return false;
    }

    MYSQL* mysql = nullptr;
    SQLConnectionRAII mysqlcon(&mysql, sqlConnPool_);
    
    char order[256] = {0};
    snprintf(order, 256, "INSERT INTO user(username, passwd) VALUES('%s', '%s')", userName.c_str(), passwd.c_str());

    if (mysql_query(mysql, order)) {
        return false;
    }
    
    updateUserCache(userName, passwd);   // 更新缓存
    
    return true;
}

bool WebServer::sqlQuery(const char* sqlOrder, string& sqlResult) {
    // 先尝试从缓存中取数据
    if (false) {

    }

    MYSQL* mysql = nullptr;
    SQLConnectionRAII mysqlcon(&mysql, sqlConnPool_);

    MYSQL_FIELD* fields = nullptr;
    MYSQL_RES* res = nullptr;
    unsigned int num_fields;

    if (mysql_query(mysql, sqlOrder)) {
        mysql_free_result(res);
        return false;
    }

    res = mysql_store_result(mysql);
    fields = mysql_fetch_fields(res);
    num_fields = mysql_num_fields(res);

    for (int i = 0; i < num_fields; ++i) {
        sqlResult += fields[i].name;
        sqlResult.push_back(',');
    }
    sqlResult.pop_back();
    sqlResult += "<br/>\n";

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        unsigned long* lengths;
        lengths = mysql_fetch_lengths(res);
        for (int i = 0; i < num_fields; ++i) {
            sqlResult += (row[i] ? row[i] : "NULL");
            sqlResult.push_back(',');
            DEBUG_INFO(cout << "row " << i << ": " << (int)lengths[i] << ", " << (row[i] ? row[i] : "NULL") << " ");
        }
        sqlResult.pop_back();
        sqlResult += "<br/>\n";
        DEBUG_INFO(cout << endl);
    }
    
    mysql_free_result(res);
    
    return true;
}

void WebServer::updateUserCache(const string& userName, const string& passwd) {
    lock_guard<mutex> locker(userCacheMtx_);
    userCache_.emplace(userName, passwd);
}

// 监听事件
bool WebServer::dealListenEvent() {
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    int connfd = accept(listenfd_, (struct sockaddr *)&client_address, &client_addrlength);
    if (connfd < 0) {
        LOG_ERROR("%s:errno is:%d", "accept error", errno);
        return false;
    }
    if (HttpConnection::userCount >= maxHttpConns_) {
        LOG_ERROR("%s", "Internal server busy");
        return false;
    }
    
    // 初始化HTTP连接
    users_[connfd].init(client_address, connfd, 
                    bind(&WebServer::loginVerify, this, placeholders::_1, placeholders::_2), 
                    bind(&WebServer::registerVerify, this, placeholders::_1, placeholders::_2),
                    bind(&WebServer::sqlQuery, this, placeholders::_1, placeholders::_2));
    // 注册读事件
    epoller_->addfd(connfd, HttpConnection::isEt, true, true);

    // 初始化client_data数据
    usersData_[connfd].address = client_address;
    usersData_[connfd].sockfd = connfd;

    // 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    util_timer *timer = new util_timer;
    timer->update();

    usersData_[connfd].timer = timer;
    timers_->add_timer(timer, bind(&WebServer::closeConnection_, this, connfd));

    return true;
}

// 读事件
bool WebServer::dealReadEvent(int sockfd) {
    util_timer *timer = usersData_[sockfd].timer;

    int readErrno = 0;
    ssize_t len = users_[sockfd].httpRead(&readErrno);

    // 若接收失败, 则关闭TCP连接 (=0说明对方断开连接)
    if (len <= 0 && (readErrno != EAGAIN && readErrno != EWOULDBLOCK)) {
        closeConnection(sockfd);
        return false;
    }

    threadPool_->append(bind(&WebServer::process, this, sockfd));

    if (timer) {
        timer->update();
        timers_->adjust_timer(timer);
    }

    return true;
}

// 写事件
bool WebServer::dealWriteEvent(int sockfd) {
    util_timer *timer = usersData_[sockfd].timer;

    int writeErrno = 0;
    ssize_t len = users_[sockfd].httpWrite(&writeErrno);

    // 若发送失败, 则关闭TCP连接
    if (len < 0 && writeErrno != EAGAIN) {
        users_[sockfd].unmap();
        closeConnection(sockfd);
        return false;
    }

    // 发送完毕
    if (users_[sockfd].toWriteBytes() == 0) {
        users_[sockfd].unmap();
        epoller_->modfd(sockfd, EPOLLIN, HttpConnection::isEt, true);
        // 判断是否是长连接
        if (!users_[sockfd].isKeepAlive()) {
            closeConnection(sockfd);
            return false;
        }
        users_[sockfd].init();
    }
    // 继续传输
    else if ((len < 0 && writeErrno == EAGAIN) || len > 0) {
        epoller_->modfd(sockfd, EPOLLOUT, HttpConnection::isEt, true);
    }

    if (timer) {
        timer->update();
        timers_->adjust_timer(timer);
    }

    return true;
}

void WebServer::closeConnection(int sockfd) {
    // 将该通信描述符从epoll监听事件中移除, 并关闭通信描述符
    closeConnection_(sockfd);

    // 移除对应的定时器
    util_timer *timer = usersData_[sockfd].timer;
    if (timer) timers_->del_timer(timer);
}

void WebServer::closeConnection_(int sockfd) {
    epoller_->removefd(sockfd);
    users_[sockfd].closeConnection();
}

void WebServer::process(int sockfd) {
    if (users_[sockfd].httpProcess())
        epoller_->modfd(sockfd, EPOLLOUT, HttpConnection::isEt, true);
    else
        epoller_->modfd(sockfd, EPOLLIN, HttpConnection::isEt, true);
}

// Webserver核心逻辑
void WebServer::start() {
    int timeout = -1;

    while (!isStop_) {
        // 检测TCP心跳
        timeout = timers_->doTick();

        // 等待EPOLL事件
        int eventsNum = epoller_->wait(timeout);
        if (eventsNum < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < eventsNum; i++) {
            uint32_t trigerEvent = epoller_->getEvent(i);
            int sockfd = epoller_->getEventfd(i);
            // 监听事件
            if (sockfd == listenfd_) {
                dealListenEvent();
            }
            // 错误事件
            else if (trigerEvent & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                closeConnection(sockfd);
            }
            // 读事件
            else if (trigerEvent & EPOLLIN) {
                dealReadEvent(sockfd);
            }
            // 写事件
            else if (trigerEvent & EPOLLOUT) {
                dealWriteEvent(sockfd);
            }
            // 其它事件
            else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}