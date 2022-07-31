#include "../include/webserver.h"

WebServer::WebServer(int port, int timeout, bool isAsynLog, string sqlUser, string sqlPasswd, string sqlDataBaseName) : 
    port_(port), epoller_(new Epoller()) {

    // 初始化监听描述符
    InitListenSocket();

    timers_ = new TimerMinHeap(timeout);

    // 初始化数据库连接池
    sqlConnPool_ = connection_pool::GetInstance();
    sqlConnPool_->init("localhost", sqlUser, sqlPasswd, sqlDataBaseName, 3306, 8);
    
    // 初始化HTTP连接池
    users_ = new http_conn[maxHttpConns_];
    usersData_ = new client_data[maxHttpConns_];

    // 初始化数据库读取表
    users_->initmysql_result(sqlConnPool_);

    // 初始化线程池
    threadPool_ = new ThreadPool<http_conn>(sqlConnPool_);

    // 初始化日志
    if (isAsynLog)
        Log::get_instance()->init("ServerLog", 2000, 800000, 8);  // 异步日志模型
    else
        Log::get_instance()->init("ServerLog", 2000, 800000, 0);  // 同步日志模型
}

WebServer::~WebServer() {
    close(listenfd_);

    delete epoller_;
    delete timers_;
    delete threadPool_;
    // delete sqlConnPool_;

    delete[] users_;
    delete[] usersData_;
}

void WebServer::InitListenSocket() {
    listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd_ >= 0);

    // 监听listenfd事件
    epoller_->addfd(listenfd_, false);

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

// 监听事件
bool WebServer::dealListenEvent() {
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    int connfd = accept(listenfd_, (struct sockaddr *)&client_address, &client_addrlength);
    if (connfd < 0) {
        LOG_ERROR("%s:errno is:%d", "accept error", errno);
        return false;
    }
    if (http_conn::m_user_count >= maxHttpConns_) {
        show_error(connfd, "Internal server busy");
        LOG_ERROR("%s", "Internal server busy");
        return false;
    }
    
    // 初始化HTTP连接
    users_[connfd].init(connfd, client_address, epoller_);

    printClientInfo(client_address, true);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    usersData_[connfd].address = client_address;
    usersData_[connfd].sockfd = connfd;

    util_timer *timer = new util_timer;
    timer->user_data = &usersData_[connfd];
    // timer->cb_func = cb_func;
    timer->update();

    usersData_[connfd].timer = timer;

    // bind(&closeConnection_, this, &usersData_[connfd]) -> false
    timers_->add_timer(timer, bind(&WebServer::closeConnection_, this, &usersData_[connfd]));

    return true;
}

// 读事件
bool WebServer::dealReadEvent(int sockfd) {
    util_timer *timer = usersData_[sockfd].timer;

    // 将客户端请求读取到读缓存中
    if (users_[sockfd].read_once()) {
        LOG_INFO("deal with the client(%s)", inet_ntoa(users_[sockfd].get_address()->sin_addr));
        Log::get_instance()->flush();

        // 若监测到读事件，将该事件放入请求队列
        threadPool_->append(users_ + sockfd);

        // 若有数据传输，则将定时器往后延迟3个单位
        if (timer) {
            timer->update();
            LOG_INFO("%s", "adjust timer once");
            Log::get_instance()->flush();

            timers_->adjust_timer(timer);
        }

        return true;
    }

    // 若读取失败, 则调用定时器回调函数来关闭TCP连接
    closeConnection(sockfd);

    return false;
}

// 写事件
bool WebServer::dealWriteEvent(int sockfd) {
    util_timer *timer = usersData_[sockfd].timer;

    // 发送数据
    if (users_[sockfd].write()) {
        LOG_INFO("send data to the client(%s)", inet_ntoa(users_[sockfd].get_address()->sin_addr));
        Log::get_instance()->flush();

        // 若有数据传输，则将定时器往后延迟3个单位
        if (timer) {
            timer->update();
            LOG_INFO("%s", "adjust timer once");
            Log::get_instance()->flush();

            timers_->adjust_timer(timer);
        }

        return true;
    }

    // 若发送失败, 则调用定时器回调函数来关闭TCP连接
    closeConnection(sockfd);

    return false;
}

void WebServer::closeConnection(int sockfd) {
    // 将该通信描述符从epoll监听事件中移除, 并关闭通信描述符
    // util_timer *timer = usersData_[sockfd].timer;
    // timer->cb_func(&usersData_[sockfd]);

    closeConnection_(&usersData_[sockfd]);

    // 移除对应的定时器
    util_timer *timer = usersData_[sockfd].timer;
    if (timer) timers_->del_timer(timer);
}

void WebServer::closeConnection_(client_data *user_data) {
    epoller_->removefd(user_data->sockfd);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;

    LOG_INFO("close fd %d", user_data->sockfd);
    Log::get_instance()->flush();
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