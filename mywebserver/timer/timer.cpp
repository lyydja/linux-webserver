#include "timer.h"

SortTimerLst::SortTimerLst()
{
    head = NULL;
    tail = NULL;
}
SortTimerLst::~SortTimerLst()
{
    UtilTimer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void SortTimerLst::AddTimer(UtilTimer *timer)
{
    if (!timer)
    {
        return;
    }
    if (!head)
    {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    AddTimer(timer, head);
}
void SortTimerLst::AdjustTimer(UtilTimer *timer)
{
    if (!timer)
    {
        return;
    }
    UtilTimer *tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        AddTimer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        AddTimer(timer, timer->next);
    }
}
void SortTimerLst::DelTimer(UtilTimer *timer)
{
    if (!timer)
    {
        return;
    }
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}
void SortTimerLst::Tick()
{
    if (!head)
    {
        return;
    }
    
    time_t cur = time(NULL);
    UtilTimer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }
        tmp->CbFunc(tmp->user_data);
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void SortTimerLst::AddTimer(UtilTimer *timer, UtilTimer *lst_head)
{
    UtilTimer *prev = lst_head;
    UtilTimer *tmp = prev->next;
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::Init(int timeslot) {
    m_TIMESLOT = timeslot;
}

void Utils::SigHandler(int sig) {
    //保留原来的errno，在函数最后恢复，以保证函数的可重入性
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char*)&msg, 1, 0); //将信号值写入管道，以通知主循环
    errno = save_errno;
}

void Utils::TimerHandler(){
    //定时处理任务，实际上就是调用tick函数
    m_timer_lst.Tick();
    //因为一次alarm调用只会引起一次SIGALRM信号，所以我们要重新定时，以不断触发SIGALRM信号
    alarm(m_TIMESLOT);
}

//添加信号捕捉
void Utils::AddSig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }      
    sigfillset(&sa.sa_mask); //在信号集中设置所有的信号
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::ShowError(int connfd, const char* info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

//将文件描述符设置为非阻塞的
int Utils::SetnonBlocking(int fd) {
    int old_option = fcntl(fd, F_GETFL); //获取文件描述符旧的状态标志
    int new_option = old_option | O_NONBLOCK; //设置非阻塞标志
    fcntl(fd, F_SETFL, new_option); 
    return old_option; //返回文件描述符旧的状态标志，以便日后恢复该状态标志
}

//将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中
//参数oneshot指定是否注册fd上的EPOLLONESHOT事件
void Utils::AddFd(int epollfd, int fd, bool one_shot) {
    AddFd(epollfd, fd, one_shot, 1);
}

/* 将内核时间表注册读事件， ET模式，选择开启 EPOLLONESHOT */
void Utils::AddFd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (TRIGMode == 1) {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    SetnonBlocking(fd);
}

void Utils::Removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

//修改文件描述符
void Utils::ModFd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLET | EPOLLONESHOT | EPOLLRDHUP | ev;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
//定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之
void CbFunc(ClientData* user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_userCount--;
}