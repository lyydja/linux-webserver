#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <string>
#include <signal.h>

#include "../threadpool/threadpool.h"
#include "../http/http_conn.h"
#include "../log/log.h"
#include "../timer/timer.h"
#include "../CGIMysql/sql_conntion_pool.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();

    /* 初始化 */
    void Init(int port, string user, string passwd, string databaseName, int log_write, int thread_num, int opt_linger, int trigmode, int sqlNum, int close_log, int actormodel);
    void TrigMode();

    void SqlPool();
    void Threadpool();
    void LogStart();
    void EventListen();
    void Timer(int connfd, const sockaddr_in &addr);
    bool DealClientSituations();
    void AdjustTimer(UtilTimer* timer);
    void DealTimer(UtilTimer* timer, int sockfd);
    bool DealSignal(bool& timeout, bool& stop_server);
    void DealRead(int sockfd);
    void DealWrite(int sockfd);
    void EventLoop();


public:
    int m_port;
    char* m_root;
    int m_actorModel;

    int m_logWrite;
    int m_closeLog;

    int m_pipefd[2];
    int m_epollfd;
    http_conn* users;

    //线程池相关
    ThreadPool<http_conn>* m_pool;
    int m_threadNum;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];
    int m_listenfd;
    int m_optLinger;
    int m_tirgMode;
    int m_listenTrigmode;
    int m_connTrigmode;

    //数据库类相关
    string m_user;
    string m_passWd;
    string m_databaseName;
    int m_sqlNum;
    ConnectionPool* m_connPool;
    //定时器类
    ClientData* users_timer;
    Utils utils;

};
#endif
