#ifndef _CONNECTION_POLL_H_
#define _CONNECTION_POLL_H_

#include <stdio.h>
#include <iostream>
#include <list>
#include <mysql/mysql.h>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

//数据库连接池类，用来存储连接数据库的用户信息
class ConnectionPool {
public:
    //获取数据库的连接
    MYSQL* GetConnection();
    //释放数据库连接
    bool ReleaseConnection(MYSQL* conn);
    //获取空闲连接
    int GetFreeConn();
    //销毁所有连接
    void DestroyPool();

    //单例模式
    static ConnectionPool* GetInstance();
    //初始化
    void Init(string url, string user, string passwd, string databaseName, int port, int maxConn, int closeLog);

private:
    ConnectionPool();
    ~ConnectionPool();

public:
    string m_url; //主机地址
    string m_user; //登陆数据库的用户名
    string m_passwd; //登陆数据库的密码
    string m_databaseName; //登陆数据库的库名
    int m_port; //数据库端口号
    int m_closeLog; //日志开关

private:
    list<MYSQL*> connList; //连接池
    int m_maxConn;  //最大的连接用户数
    int m_freeConn; //当前空闲的连接数
    int m_currConn; //当前已使用的连接数

    Locker lock;
    Sem sem;
};


class ConnectionRAII {
public:
    ConnectionRAII(MYSQL** conn, ConnectionPool* connPool);
    ~ConnectionRAII();
private:
    MYSQL* connRAII;
    ConnectionPool* poolRAII;
};

#endif