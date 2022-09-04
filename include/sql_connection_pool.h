#ifndef _SQLConnectionPool_H
#define _SQLConnectionPool_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <mutex>
#include <condition_variable>

#ifdef DEBUG_MODE
#define DEBUG_INFO(x) x
#else
#define DEBUG_INFO(x)
#endif

using namespace std;

class SQLConnectionPool {
public:
	MYSQL* GetConnection();				    //获取数据库连接
	bool ReleaseConnection(MYSQL *sql);     //释放连接
	void DestroyPool();					    //销毁所有连接

	// 单例模式
	static SQLConnectionPool* GetInstance();

	void init(string url, string user, string passWord, string DBName, unsigned int port, unsigned int maxConn); 
	
private:
	SQLConnectionPool() = default;
	~SQLConnectionPool() { DestroyPool(); }

private:
	unsigned int MaxConn;           //最大连接数

private:
	list<MYSQL*> connList_;         //连接池
    mutex mtx_;
    condition_variable cond_;

private:
	string url_;			        //主机地址
	unsigned int port_;		        //数据库端口号
	string user_;		            //登陆数据库用户名
	string passWord_;	            //登陆数据库密码
	string databaseName_;           //使用数据库名
};

class SQLConnectionRAII {
public:
    // sql是一个传出参数, 为的是可以改变指针的值
	SQLConnectionRAII(MYSQL** sql, SQLConnectionPool* connPool);
	~SQLConnectionRAII();
	
private:
	MYSQL* sqlRAII;                         // 数据库连接池的一个连接
	SQLConnectionPool* poolRAII;            // 数据库连接池
};

#endif