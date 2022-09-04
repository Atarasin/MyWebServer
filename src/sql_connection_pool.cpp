#include "../include/sql_connection_pool.h"

SQLConnectionPool* SQLConnectionPool::GetInstance() {
	static SQLConnectionPool connPool;
	return &connPool;
}

void SQLConnectionPool::init(string url, string user, string passWord, string DBName, unsigned int port, unsigned int maxConn) {
	url_ = url;
	port_ = port;
	user_ = user;
	passWord_ = passWord;
	databaseName_ = DBName;
    MaxConn = maxConn;

    // 建立一定数量MaxConn的数据库连接
	for (int i = 0; i < maxConn; i++) {
		MYSQL* con = NULL;
		con = mysql_init(con);      // 初始化mysql连接
		if (con == NULL) {
			DEBUG_INFO(cout << "Error:" << mysql_error(con));
			exit(1);
		}

        // 建立一个到mysql数据库的连接
		con = mysql_real_connect(con, url_.c_str(), user_.c_str(), passWord_.c_str(), databaseName_.c_str(), port_, NULL, 0);
		if (con == NULL) {
			DEBUG_INFO(cout << "Error:" << mysql_error(con));
			exit(1);
		}

        // 设置mysql字符集 (避免查询数据在网页上显示时出现乱码)
        mysql_set_character_set(con, "utf8");

		connList_.push_back(con);
	}
}

// customer
MYSQL* SQLConnectionPool::GetConnection() {
    unique_lock<mutex> locker(mtx_);

    while (connList_.empty()) {
        cond_.wait(locker);
    }

    MYSQL* con = connList_.front();
    connList_.pop_back();

	return con;
}

// producer
bool SQLConnectionPool::ReleaseConnection(MYSQL* con) {
	if (con == nullptr) return false;

    {
        lock_guard<mutex> locker(mtx_);
        connList_.push_back(con);
    }
    cond_.notify_one();

    return true;
}

// 销毁数据库连接池
void SQLConnectionPool::DestroyPool() {
    lock_guard<mutex> locker(mtx_);

	if (!connList_.empty()) {
		list<MYSQL*>::iterator it;
		for (it = connList_.begin(); it != connList_.end(); ++it) {
			MYSQL* con = *it;
			mysql_close(con);
		}
		connList_.clear();
	}
}

SQLConnectionRAII::SQLConnectionRAII(MYSQL** sql, SQLConnectionPool* connPool) {
	*sql = connPool->GetConnection();
	sqlRAII = *sql;
	poolRAII = connPool;
}

SQLConnectionRAII::~SQLConnectionRAII() {
	poolRAII->ReleaseConnection(sqlRAII);
}