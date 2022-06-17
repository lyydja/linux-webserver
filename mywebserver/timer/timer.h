#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "../http/http_conn.h"

#define BUFFER_SIZE 64

class UtilTimer;//类声明

struct ClientData
{
    sockaddr_in address;
    int sockfd;
    UtilTimer *timer;
};

class UtilTimer
{
public:
    UtilTimer() : prev(NULL), next(NULL) {}

public:
    time_t expire;
    
    void (* CbFunc)(ClientData *); //定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之
    ClientData *user_data;
    UtilTimer *prev;
    UtilTimer *next;
};

class SortTimerLst
{
public:
    SortTimerLst();
    ~SortTimerLst();

    void AddTimer(UtilTimer *timer);
    void AdjustTimer(UtilTimer *timer);
    void DelTimer(UtilTimer *timer);
    void Tick();

private:
    void AddTimer(UtilTimer *timer, UtilTimer *lst_head);

    UtilTimer *head;
    UtilTimer *tail;
};


class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void Init(int timeslot);

    //对文件描述符设置非阻塞
    int SetnonBlocking(int fd);
    //将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中
    //参数one_shot指定是否注册fd上的EPOLLONESHOT事件
    void AddFd(int epollfd, int fd, bool one_shot);
    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void AddFd(int epollfd, int fd, bool one_shot, int TRIGMode);
    void Removefd(int epollfd, int fd);
    void ModFd(int epollfd, int fd, int ev);
    //设置信号函数
    void AddSig(int sig, void(handler)(int), bool restart = true);
    void ShowError(int connfd, const char *info);
    //信号处理函数
    static void SigHandler(int sig);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void TimerHandler();

public:
    static int *u_pipefd;
    SortTimerLst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void CbFunc(ClientData* user_data); //定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之


#endif
