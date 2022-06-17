#include "webserver.h"

WebServer::WebServer()
{
    //http_conn类对象
    users = new http_conn[MAX_FD];
    //获取root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char*)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);
    /* 定时器 */
    users_timer = new ClientData[MAX_FD];
    
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete m_pool;
}

void WebServer::Init(int port, string user, string passwd, string databaseName, int log_write, int thread_num, int opt_linger, int trigmode, int sqlNum, int close_log, int actormodel) {
    m_port = port;
    m_user = user;
    m_passWd = passwd;
    m_databaseName = databaseName;
    m_sqlNum = sqlNum;
    m_logWrite = log_write;
    m_threadNum = thread_num;
    m_optLinger = opt_linger;
    m_tirgMode = trigmode;
    m_listenTrigmode = 0;
    m_connTrigmode = 0; //.................
    m_closeLog = close_log;
    m_actorModel = actormodel;
}

void WebServer::TrigMode()
{
    //LT + LT
    if (0 == m_tirgMode)
    {
        m_listenTrigmode = 0;
        m_connTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_tirgMode)
    {
        m_listenTrigmode = 0;
        m_connTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_tirgMode)
    {
        m_listenTrigmode = 1;
        m_connTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_tirgMode)
    {
        m_listenTrigmode = 1;
        m_connTrigmode = 1;
    }
}

void WebServer::LogStart()
{
    if (0 == m_closeLog)
    {
        //初始化日志
        //异步
        if (1 == m_logWrite)
            Log::GetInstance()->Init("./var/ServerLog", m_closeLog, 2000, 800000, 800);
        //同步
        else
            Log::GetInstance()->Init("./var/ServerLog", m_closeLog, 2000, 800000, 0);
    }
}

void WebServer::SqlPool() {
    //初始化数据库连接池
    m_connPool = ConnectionPool::GetInstance();
    m_connPool->Init("localhost", m_user, m_passWd, m_databaseName, 3306, m_sqlNum, m_closeLog);
    //初始化数据库读取表
    users->InitMysqlResult(m_connPool);
}

void WebServer::Threadpool()
{
    //创建线程池，初始化线程池
    m_pool = new ThreadPool<http_conn>(m_actorModel, m_connPool, m_threadNum);
}

//服务器端创建监听的套接字
void WebServer::EventListen()
{
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if(m_listenfd == -1) {
        LOG_ERROR("%s", "socket failure");
        exit(1);
    }
    
    //优雅的关闭
    if(0 == m_optLinger) {
        struct linger tmp = {0, 1}; //SO_LINGER不起作用，close用默认行为关闭socket
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if(1 == m_optLinger) {
        struct linger tmp = {1, 1}; //开启该选项，延迟时间为1（阻塞的socket才行） 
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    
    //端口复用，仅用于测试，实际中不需要
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    //绑定
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port); //默认80端口，但是需要root下运行
    ret = bind(m_listenfd, (struct sockaddr*)& address, sizeof(address));
    if(ret == -1) {
        LOG_ERROR("%s", "bind failure");
        exit(1);
    }
    //监听
    ret = listen(m_listenfd, 5);
    if(ret == -1) {
        LOG_ERROR("%s", "listen failure");
    }

    utils.Init(TIMESLOT);
    
    //创建epoll对象，事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);  

    http_conn::m_epollfd = m_epollfd;
    //将监听的文件描述符添加到epoll对象中
    // utils.AddFd(m_epollfd, m_listenfd, false);
    utils.AddFd(m_epollfd, m_listenfd, false, m_listenTrigmode);

    //使用socketpair创建管道，注册m_pipefd[0]上可读事件
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.SetnonBlocking(m_pipefd[1]);
    utils.AddFd(m_epollfd, m_pipefd[0], false, 0);

    
    utils.AddSig(SIGPIPE, SIG_IGN);//忽略SIGPIPE信号
    utils.AddSig(SIGALRM, utils.SigHandler, false); //处理ALARM信号
    utils.AddSig(SIGTERM, utils.SigHandler, false); //处理TERM信号（kill命令信号）

    alarm(TIMESLOT);

    /* 工具类，信号和描述符基础操作 */
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd; 
}

//连接成功，初始化该连接并且创建定时器
void WebServer::Timer(int connfd, const sockaddr_in &client_address) {
    //将新的客户的数据初始化，放到数组中
    users[connfd].Init(connfd, client_address, m_root, m_connTrigmode, m_closeLog, m_user, m_passWd, m_databaseName);

    //初始化定时器相关数据
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    UtilTimer* timer = new UtilTimer;
    timer->user_data = &users_timer[connfd];
    timer->CbFunc = CbFunc;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.AddTimer(timer);
}

//客户端连接情况
bool WebServer::DealClientSituations() {
    struct sockaddr_in client_address;
    socklen_t client_addrlen = sizeof(client_address);

    //LT，水平触发
    if(0 == m_listenTrigmode) {
        int connfd = accept(m_listenfd, (struct sockaddr*)& client_address, &client_addrlen);
        if(connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            cout << "accept error" << endl;
            return false;
        }

        if(http_conn::m_userCount >= MAX_FD) {
            //目前连接数满了
            //给客户端写一个信息：服务器内部正忙。
            utils.ShowError(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        //连接成功，初始化该连接并且创建定时器
        Timer(connfd, client_address);
    }
    //ET，边沿触发
    else {
        while(1) {
            int connfd = accept(m_listenfd, (struct sockaddr*)& client_address, &client_addrlen);
            if(connfd < 0) {
                cout << "accept error" << endl;
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                return false;
            }

            if(http_conn::m_userCount >= MAX_FD) {
                //目前连接数满了
                //给客户端写一个信息：服务器内部正忙。
                utils.ShowError(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                return false;
            }
            //连接成功，初始化该连接并且创建定时器
            Timer(connfd, client_address);
        }
        return false;
    }

    return true;    
}

//修改定时器事件
void WebServer::AdjustTimer(UtilTimer* timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.AdjustTimer(timer);

    LOG_INFO("%s", "adjust timer once");
}

//定时器到期，关闭连接，并移除其对应的定时器
void WebServer::DealTimer(UtilTimer* timer, int sockfd) {
    timer->CbFunc(&users_timer[sockfd]); //关闭连接
    //移除定时器
    if(timer) {
        utils.m_timer_lst.DelTimer(timer);
    }
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

//处理SIGALAM和SIGTETM信号
bool WebServer::DealSignal(bool& timeout, bool& stop_server) {
    int ret;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1) {
        return false;
    }
    else if(ret == 0) {
        return false;
    }
    else {
        for(int i = 0; i < ret; i++) {
            switch (signals[i])
            {
                case SIGALRM: {
                    //用timeout变量标记有定时任务需要处理，但不立即处理定时任务。
                    //这是因为定时任务的优先级不是很高，我们优先处理其他更重要的额任务
                    timeout = true;
                    break;
                }
                case SIGTERM: {
                    stop_server = true;
                    break;
                }
            }
        }
    }

    return true;
}

//处理读事件
void WebServer::DealRead(int sockfd) {
    UtilTimer* timer = users_timer[sockfd].timer;
    //reactor模式
    if(1 == m_actorModel) {
        //将定时器延迟
        if(timer) {
            AdjustTimer(timer);
        }
        //检测到读事件，将该事件放入请求队列
        m_pool->Append(users + sockfd, 0);

        while(true) {
            if(1 == users[sockfd].improv ) {

                if(1 == users[sockfd].timerFlag) {
                    DealTimer(timer, sockfd);
                    users[sockfd].timerFlag = 0;
                }

                users[sockfd].improv = 0;
                break;
            }
        }
    }
    //模拟proactor模式
    else {
        //根据读的结果，决定是将任务添加到线程池，还是关闭连接
        if(users[sockfd].Read()) {
            //一次性把所有数据都读完
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].GetAddress()->sin_addr));
            m_pool->AppendProactor(users + sockfd);
            if(timer) {
                AdjustTimer(timer);
            }
        }
        else {
            //断开连接并清除定时器
            DealTimer(timer, sockfd);
        }
    }
}

//处理写事件
void WebServer::DealWrite(int sockfd) {

    UtilTimer* timer = users_timer[sockfd].timer;
    //reactor模式
    if(1 == m_actorModel) {
        if(timer) {
            AdjustTimer(timer);
        }
        m_pool->Append(users + sockfd, 1);

        while(true) {
            if(1 == users[sockfd].improv) {
                if(1 == users[sockfd].timerFlag) {
                    DealTimer(timer, sockfd);
                    users[sockfd].timerFlag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    //模拟proactor模式
    else {

        if(users[sockfd].Write()) {    //一次性写完所有数据
            LOG_INFO("send data to the client(%s)",inet_ntoa(users[sockfd].GetAddress()->sin_addr));
            if(timer) {
                AdjustTimer(timer);
            }
        }
        else {
            //断开连接并清除定时器
            DealTimer(timer, sockfd);
        }
    }
}


void WebServer::EventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int num = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if((num < 0) && (errno != EINTR)) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        //循环遍历事件数组
        for(int i = 0; i < num; i++) {
            int sockfd = events[i].data.fd;

            //有客户端连接进来
            if(sockfd == m_listenfd) {
                bool flag = DealClientSituations();      
                if(flag == false)   continue;
            }
            //对方异常断开或者错误等事件
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                DealTimer(users_timer[sockfd].timer, sockfd);
            }
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
                bool flag = DealSignal(timeout, stop_server);
                if(flag == false) {
                    LOG_ERROR("%s", "dealclientsituations failure");
                }
            }
            //处理读事件
            else if(events[i].events & EPOLLIN) {
                DealRead(sockfd);
            }
            else if(events[i].events & EPOLLOUT) {
                DealWrite(sockfd);
            }
            
        }
        /******************下面代码的位置待探究********************/
            //最后处理定时事件，因为I/O事件有更高的的优先级。
            //当然，这样做将导致定时任务不能精确的按照预期的时间执行
            if(timeout) {
                utils.TimerHandler();
                LOG_INFO("%s", "timer tick");
                timeout = false;
            }
        /********************************************************/
        
    }
}