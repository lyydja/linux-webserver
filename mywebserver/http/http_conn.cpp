#include "http_conn.h"

/*定义HTTP响应的一些状态信息*/
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requeseted file was not found on this server.\n";
const char* error_500_title = "Internal ERROR";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";



map<string, string> users;
Locker m_lock;
//初始化数据库结果集
void http_conn::InitMysqlResult(ConnectionPool* connPool) {
    //先从连接池中取一个连接
    MYSQL* mysql = NULL;
    ConnectionRAII mySqlConn(&mysql, connPool);

    //在user表中检索username, passwd数据，浏览器端输入
    //返回非0值，说明有错误
    if(mysql_query(mysql, "SELECT username, passwd FROM user")) {
        LOG_ERROR("SELECT error: %s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);
    
    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有行的数组
    MYSQL_FIELD* fileds = mysql_fetch_field(result);

    //从结果集中获取下一行，将对应的用户名、密码，存入map中，便于有客户登陆时验证
    while(MYSQL_ROW row = mysql_fetch_row(result)) {
        string tmp1(row[0]); //用户名
        string tmp2(row[1]); //密码
        users[tmp1] = tmp2;
    }
    //后边还需要操作这一块内存，所以不能用mysql_free_result来释放内存空间
}   

//将文件描述符设置为非阻塞的
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL); //获取文件描述符旧的状态标志
    int new_option = old_option | O_NONBLOCK; //设置非阻塞标志
    fcntl(fd, F_SETFL, new_option); 
    return old_option; //返回文件描述符旧的状态标志，以便日后恢复该状态标志
}

/* 将内核时间表注册读事件， ET模式，选择开启 EPOLLONESHOT */
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
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
    setnonblocking(fd);
}

// //将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中
// //参数oneshot指定是否注册fd上的EPOLLONESHOT事件
// void addfd(int epollfd, int fd, bool one_shot) {
//     addfd(epollfd, fd, one_shot, 1);
// }

//从内核时间表删除描述符
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}
//修改文件描述符
void modfd(int epollfd, int fd, int ev, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;
    if(1 == TRIGMode) {
        event.events = EPOLLET | EPOLLONESHOT | EPOLLRDHUP | ev;
    }
    else {
        event.events = EPOLLONESHOT | EPOLLRDHUP | ev;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// //修改文件描述符
// void modfd(int epollfd, int fd, int ev) {
//     epoll_event event;
//     event.data.fd = fd;
//     event.events = EPOLLET | EPOLLONESHOT | EPOLLRDHUP | ev;
//     epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
// }



int http_conn::m_epollfd = -1;
int http_conn::m_userCount = 0;

//关闭客户端的连接
void http_conn::CloseConn(bool real_close) {
    if(real_close && m_socketfd != -1) {
        printf("close %d\n", m_socketfd);
        removefd(m_epollfd, m_socketfd);
        m_socketfd = -1;
        m_userCount--; //关闭一个连接时，客户总数量-1
    }
}

//初始化
void http_conn::Init()
{
    mysql = NULL;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_method = GET;
    m_readIndex = 0;
    m_writeIndex = 0;
    m_startLine = 0;
    m_checkIndex = 0;
    m_url = 0;
    m_version = 0;
    m_contentLength = 0;
    m_linger = false;
    m_host = 0;
    m_printfFlag = false;
    bytesToSend = 0;
    bytesHaveSend = 0;

    cgi = 0;
    m_state = 0;
    timerFlag = 0;
    improv = 0;

    memset(m_readBuf, '\0', READ_BUF_SIZE);
    memset(m_writeBuf, '\0', WRITE_BUF_SIZE);
    memset(m_realFile, '\0', FILENAME_LEN);
}

//初始化新接受的请求
void http_conn::Init(int socketfd, const sockaddr_in &addr, char* root, int TRIGMode, int close_log, string user, string passwd, string sqlname)
{
    m_socketfd = socketfd;
    m_address = addr;
    
    addfd(m_epollfd, socketfd, true, m_TRIGMode);
    m_userCount++; //添加文件描述符之后，用户数量加1

    //当浏览器出现连接重置时，可能是网站根目录出错或http相应格式出错或文件中的内容完全为空
    docRoot = root;
    m_TRIGMode = TRIGMode;
    m_closeLog = close_log;

    strcpy(m_sqlUser, user.c_str());
    strcpy(m_sqlPasswd, passwd.c_str());
    strcpy(m_sqlName, sqlname.c_str());

    Init();
}

//一次性读完请求数据
bool http_conn::Read()
{
    if(m_readIndex >= READ_BUF_SIZE) {
        return false;
    }
    //读取到的字节
    int bytes_read = 0;

    //LT读取数据
    if(0 == m_TRIGMode) {
        bytes_read = recv(m_socketfd, m_readBuf + m_readIndex, READ_BUF_SIZE - m_readIndex, 0);
        //更新位置
        m_readIndex += bytes_read;
        if(bytes_read <= 0) {
            return false;
        }
        // printf("接收到了数据：%s\n", m_read_buf);
        return true;
    }
    //ET读数据
    else {
        while (true) {
            //接收数据
            bytes_read = recv(m_socketfd, m_readBuf + m_readIndex, READ_BUF_SIZE - m_readIndex, 0);
            if(bytes_read == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                return false;
            }
            else if(bytes_read == 0) {
                //对方关闭了连接
                return false;
            }
            //更新位置
            m_readIndex += bytes_read;
        }
        // printf("接收到了数据：%s\n", m_read_buf);
        return true;
    }
}

//解析http请求行，获得请求方法，目标uRL，以及HTTP版本号   GET /index.html HTTP/1.1
http_conn::HTTP_CODE http_conn::ParseRequestLine(char *text) {

    m_url = strpbrk(text, " \t"); //返回字符是"\t"的指针位置, 即返回:( /index.html HTTP/1.1)
    if(!m_url) {
        return BAD_REQUEST;
    }
    *m_url++ = '\0'; //现在m_url为/index.html HTTP/1.1

    //现在获取请求的方法
    char* method = text; //上边已经用'\0'将请求方法隔开了
    if(strcasecmp(method, "GET") == 0) {
        m_method = GET;
    }
    else if(strcasecmp(method, "POST") == 0) {
        m_method = POST;
        cgi = 1;
    }
    //不支持的请求方法，暂时只支持GET和POST
    else {
        return BAD_REQUEST;
    }

    //获取url
    m_url += strspn(m_url, " \t");//因为不确定'\t'后边会不会还有'\t'，所以需要跳过这些'\t'
    m_version = strpbrk(m_url, " \t"); //现在m_url为/index.html HTTP/1.1, m_version为HTTP/1.1
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if(strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    //检查url是否合法
    if(strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if(strncasecmp(m_url, "https://", 8) == 0) {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if(!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }

    //当url为/时（GET / HTTP/1.1）,显示判断界面   
    if (strlen(m_url) == 1)
        strcat(m_url, "index.html"); //跳到默认界面

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}   

//解析头部字段               
http_conn::HTTP_CODE http_conn::ParseHeaders(char *text) {
    //遇到空行，表示头部字段解析完毕
    if(text[0] == '\0') {
        //如果http请求有消息体，则还需要读取m_content_length字节的消息体，状态机转移到CHECK_STATE_CONTENT状态
        if(m_contentLength != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }    
        return GET_REQUEST;
    }
    //处理Connection头部字段
    else if(strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0) {
            LOG_INFO("Connection: %s", text);
            // printf("Connection: %s\n", text);
            m_linger = true;
        }
    }
    //处理Content-Length头部字段
    else if(strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_contentLength = atol(text);
        LOG_INFO("Content_Length: %s", text);
        // printf("Content-Length: %s\n", text);
    }
    //处理Host头部字段
    else if(strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
        LOG_INFO("Host: %s", text);
        // printf("Host: %s\n", text);
    }
    //其他字段
    else {
        LOG_INFO("%s", text);
        // printf("%s\n", text);
    }

    return NO_REQUEST;  
}     

//解析消息体
http_conn::HTTP_CODE http_conn::ParseContent(char *text) {
    if(m_readIndex >= (m_contentLength + m_checkIndex)) {

        text[m_contentLength] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        LOG_INFO("content: %s", m_string);

        return GET_REQUEST;
    }

    return NO_REQUEST;
}

//从状态机，用于解析出一行内容
http_conn::LINE_STATUS http_conn::ParseLine() {
    //断定一行读取完事的标志是回车换行标志
    char temp;   
    for(; m_checkIndex < m_readIndex; m_checkIndex++) {
        //获得当前要分析的字节
        temp = m_readBuf[m_checkIndex];
        //如果当前的字节是'r',即回车符，则说明可能读取到了一个完整的行
        if(temp == '\r') {
            //如果当前的字节碰巧是读缓冲区中最后一个已经被读入的客户数据，那么这次没有获取到一个完整的行，需要继续读入
            if((m_checkIndex + 1) == m_readIndex) {
                return LINE_OPEN;
            }
            //如果下一个字符是'\n'，则说明读取到了一个完整的行
            else if(m_readBuf[m_checkIndex + 1] == '\n') {
                m_readBuf[m_checkIndex++] = '\0';
                m_readBuf[m_checkIndex++] = '\0';
                return LINE_OK;
            }
            //否则的话，说明客户端的http请求存在语法错误
            else {
                return LINE_BAD;
            }
        }
        //如果当前的字节是'\n'，即换行符，则也说明读取到了一个完整的行（头部字段的一个字段中不会单独出现换行符）
        else if(temp == '\n') {
            if(m_checkIndex > 1 && m_readBuf[m_checkIndex - 1] == '\r') {
                m_readBuf[m_checkIndex - 1] = '\0';
                m_readBuf[m_checkIndex++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //如果所有内容都分析完毕也没有遇到'\r'字符，还需要继续读取客户端的数据才能进一步分析
    return LINE_OPEN;
}

//当得到一个正确的http请求时，我们就分析目标文件的属性。如果目标文件存在、对所有用户可读、且不是目录，
//则使用mmap将其映射到内存地址m_file_address处，并告诉调用者调用成功
http_conn::HTTP_CODE http_conn::DoRequest() {
    strcpy(m_realFile, docRoot);
    int len = strlen(docRoot);
    
    //找到url的最后一个'/'的位置，根据'/'后边的内容来判断是进入哪个界面
    const char* p = strrchr(m_url, '/'); 
    //需要处理POST
    //'2'是点击登陆后，'3'是点击注册后，此时是POST请求，m_string中保存着提交的账号和密码
    if(cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
        //根据标志判断是登陆检测还是注册检测
        char flag = m_url[1];

        char* m_url_real = (char*) malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/"); //m_url_real = /
        //m_url = /2CGISQL.cgi或者/3CGISQL.cgi，m_url + 2 = CGISQL.cgi
        strcat(m_url_real, m_url + 2); //m_url_real = /CGISQL.cgi
        strncpy(m_realFile + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //POST响应数据格式为：user=11&password=23
        char name[100], passWd[100];
        int i;

        //提取用户名
        for(i = 5; m_string[i] != '&'; i++) {
            name[i - 5] = m_string[i];
        }
        name[i - 5] = '\0';

        //提取密码
        int j = 0;
        for(i = i + 10; m_string[i] != '\0'; i++, j++) {
            passWd[j] = m_string[i];
        }
        passWd[j] = '\0';

        //如果*(p + 1) == '3'，说明是注册新用户
        if(*(p + 1) == '3') {
            //需要检测数据库中是否有重名的
            //如果没有重名的，增加数据，处理sql语句
            char* sqlInsert = (char*) malloc(sizeof(char) * 200); //开辟空间
            strcpy(sqlInsert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sqlInsert, "'");
            strcat(sqlInsert, name);
            strcat(sqlInsert, "', '");
            strcat(sqlInsert, passWd);
            strcat(sqlInsert, "')");
            //没有重名
            if(users.find(name) == users.end()) {
                m_lock.Lock();
                int res = mysql_query(mysql, sqlInsert);
                users.insert(pair<string, string>(name, passWd));
                m_lock.Unlock();
                //注册成功
                if(!res) {
                    strcpy(m_url, "/log.html");
                }
                //注册失败，这还可以添加功能：比如：返回具体原因等等
                else {
                    strcpy(m_url, "registerError.html");
                }
            }
            //有重名
            else {
                strcpy(m_url, "/registerError.html");
            }

            // free(sqlInsert);
        }

        //如果*(p + 1) == '2'，说明是登陆
        else if(*(p + 1) == '2') {
            //如果账号密码正确
            if(users.find(name) != users.end() && users[name] == passWd) {
                strcpy(m_url, "/welcome.html");
            }
            //如果账号或密码不正确
            else {
                strcpy(m_url, "/logError.html");
            }
        }
    }
    //首页点击新用户后的界面，注册界面
    if(*(p + 1) == '0') {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        
        strcpy(m_url_real, "/register.html");
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //首页点击已有账号后的界面，登陆界面
    else if(*(p + 1) == '1') {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        
        strcpy(m_url_real, "/log.html");
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //点击xxx.jpg后的界面
    else if(*(p + 1) == '5') {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        
        strcpy(m_url_real, "/picture.html");
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //点击xxx.avi后的界面
    else if(*(p + 1) == '6') {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        
        strcpy(m_url_real, "/video.html");
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //点击关注我后的界面
    else if(*(p + 1) == '7') {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        
        strcpy(m_url_real, "/fans.html");
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else {
        strncpy(m_realFile + len, m_url, FILENAME_LEN - len - 1);
    }

    //判断目标文件是否存在
    if(stat(m_realFile, &m_fileStat) < 0) {
        return NO_RESOURCE;
    }
    //判断是否时对所有用户可读，即有无权限
    else if(!(m_fileStat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }
    //判断是否时目录
    else if(S_ISDIR(m_fileStat.st_mode)) {
        return BAD_REQUEST;
    }

    //只读
    int fd = open(m_realFile, O_RDONLY);
    m_fileAddress = (char*) mmap(0, m_fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0); //资源的内存映射
    close(fd);
    return FILE_REQUEST;
} 

//主状态机，解析http请求
http_conn::HTTP_CODE http_conn::ProcessRead() {
    //依次读取请求行、头部字段、消息体
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    LOG_INFO("%s", "got http lines: ");
    // printf("got http lines:\n");
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = ParseLine()) == LINE_OK)) {
        //获取一行数据
        text = GetLine();
        //该行数据的起始位置
        m_startLine = m_checkIndex;
        // printf("got 1 http line: %s\n", text);
        //打印请求行的内容
        if(m_printfFlag == false) {
            LOG_INFO("%s", text);
            // printf("%s\n", text);
            m_printfFlag = true;
        }

        //主状态机
        switch(m_check_state) {
            //解析请求行
            case CHECK_STATE_REQUESTLINE: {
                ret = ParseRequestLine(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            //解析头部字段
            case CHECK_STATE_HEADER: {
                ret = ParseHeaders(text);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST) {
                    return DoRequest();
                }
                break;
            }
            //解析消息体
            case CHECK_STATE_CONTENT: {
                ret = ParseContent(text);
                if(ret == GET_REQUEST) {
                    return DoRequest();
                }
                line_status = LINE_OPEN; 
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

//解除内存映射
void http_conn::UnMap() {
    if(m_fileAddress) {
        munmap(m_fileAddress, m_fileStat.st_size);
        m_fileAddress = 0;
    }
}

//写HTTP响应
bool http_conn::Write() {
    int temp = 0;

    //如果没有要发送的数据
    if(bytesToSend == 0) {
        modfd(m_epollfd, m_socketfd, EPOLLIN, m_TRIGMode);
        Init();
        return true;
    }

    //如果有要发送的数据
    while(1) {
        temp = writev(m_socketfd, m_iv, m_ivCount);
        if(temp <= -1) {
            //TCP写缓冲区没有空间了，则等待下一轮EPOLLOUT事件。虽然在此期间，服务器无法立即接受到同一客户的下一个请求，但这可以保证连接的完整性。
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                modfd(m_epollfd, m_socketfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            //如果是其他错误
            UnMap();
            return false;
        }

        bytesToSend -= temp;
        bytesHaveSend += temp;

        /*..................不太理解这段................*/
        if(bytesHaveSend >= m_iv[0].iov_len) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_fileAddress + (bytesHaveSend - m_writeIndex);
            m_iv[1].iov_len = bytesToSend;
        }
        else {
            m_iv[0].iov_base = m_writeBuf + bytesHaveSend;
            m_iv[0].iov_len = m_iv[0].iov_len - bytesHaveSend;
        }
        /*............................................*/

        if(bytesToSend <= 0) {
            UnMap();
            //发送http响应成功，根据http请求中的Connection字段决定是否立即关闭连接
            //如果保持连接
            if(m_linger) {
                modfd(m_epollfd, m_socketfd, EPOLLIN, m_TRIGMode);
                Init();
                return true;
            }
            //如果断开连接
            else {
                modfd(m_epollfd, m_socketfd, EPOLLIN, m_TRIGMode);
                return false;
            }
        }
    }
}  

//往写缓冲区中写入待发送的数据
bool http_conn::AddResponse(const char* format...) {
    //写缓冲区满了
    if(m_writeIndex >= WRITE_BUF_SIZE) {
        return false;
    }
    /*
        可以将arg_list看作一个指针
        va_start(args, formt)：将args指向第一个参数
        va_arg(args, 参数类型)：args指向下一个参数
        va_end(args)：将args置为无效
    */
    va_list arg_list; 
    va_start(arg_list, format);
    /*
        int vsnprintf(char *s, size_t n, const char *format, va_list arg)
        功能：将格式化的可变参数列表写入大小的缓冲区，如果在printf上使用格式，则使用相同的文本组成字符串，如果使用arg标识的变量则将参数列表中的元素作为字符串存储在char型指针s中。
        参数： 
            s:指向存储结果C字符串的缓冲区的指针。
            n：要转换的字符的长度（bytes）
            formate：可变参数
            arg：申请参数列表的变量，标识使用va_start初始化的变量参数列表的值。
        返回值
            如果n值勾搭，则会写入的字符数，但是不包括’\0’的终止符。如果发生编码错误则返回值小于0.
        注：只有返回值是非负而且小于n时，字符串才完全被写入。
    */
    int len = vsnprintf(m_writeBuf + m_writeIndex, WRITE_BUF_SIZE - m_writeIndex - 1, format, arg_list); //将arg_list保存在m_write_buf + m_write_index中
    if(len >= (WRITE_BUF_SIZE - m_writeIndex - 1)) {
        va_end(arg_list);
        return false;
    }
    m_writeIndex += len;
    va_end(arg_list);

    // LOG_INFO("request: %s", m_writeBuf);

    return true;
}               

//添加状态行，响应报文的第一行
bool http_conn::AddStatusLine(int status, const char* title) {
    return AddResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}   

//添加内容长度：Content-Length: xxx
bool http_conn::AddContentLength(int content_len) {
    return AddResponse("Content-Length: %d\r\n", content_len);
}  

//添加是否保持连接：Connection: keep-alive / Connection: close
bool http_conn::AddLinger() {
    return AddResponse("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

//添加回车换行
bool http_conn::AddBlankLine() {
    return AddResponse("%s", "\r\n");
}                                  

//添加头部信息（字段）
bool http_conn::AddHeaders(int content_len) {
    return AddContentLength(content_len) && AddLinger() && AddBlankLine();    
} 

//添加消息体  
bool http_conn::AddContent(const char* content) {
    return AddResponse("%s", content);
}                    
               
//根据服务器处理http请求的结果，决定返回给客户端的内容
bool http_conn::ProcessWrite(HTTP_CODE ret) {
    switch(ret) {
        //状态码：500，表示服务器本身发生错误
        case INTERNAL_ERROR: {
            AddStatusLine(500, error_500_title);
            AddHeaders(strlen(error_500_form));
            if(!AddContent(error_500_form)) {
                return false;
            }
            break;
        }
        //状态码：400，表示请求报文中存在语法错误
        case BAD_REQUEST: {
            AddStatusLine(400, error_400_title);
            AddHeaders(strlen(error_400_form));
            if(!AddContent(error_400_form)) {
                return false;
            }
            break;
        }
        //状态码：403，表示对请求资源的访问被服务器拒绝了
        case FORBIDDEN_REQUEST: {
            AddStatusLine(403, error_403_title);
            AddHeaders(strlen(error_403_form));
            if(!AddContent(error_403_form)) {
                return false;
            }
            break;
        }
        //状态码：404，表示服务器上没有请求的资源
        case NO_RESOURCE: {
            AddStatusLine(404, error_404_title);
            AddHeaders(strlen(error_404_form));
            if(!AddContent(error_404_form)) {
                return false;
            }
            break;
        }
        //状态码：200，表示请求成功
        case FILE_REQUEST: {
            AddStatusLine(200, ok_200_title);
            //如果有数据要发送
            if(m_fileStat.st_size != 0) {
                AddHeaders(m_fileStat.st_size);
                m_iv[0].iov_base = m_writeBuf; //响应报文的字段数据
                m_iv[0].iov_len = m_writeIndex;
                m_iv[1].iov_base = m_fileAddress; //请求资源的数据
                m_iv[1].iov_len = m_fileStat.st_size;
                m_ivCount = 2;
                bytesToSend = m_writeIndex + m_fileStat.st_size;
                return true;
            }
            else {
                const char* ok_string = "<html><body></body></html>";
                AddHeaders(strlen(ok_string));
                if(!AddContent(ok_string)) {
                    return false;
                }
            }
        }
        default: {
            return false;
        }

    }
    //如果没有数据要发送
    m_iv[0].iov_base = m_writeBuf;
    m_iv[0].iov_len = m_writeIndex;
    m_ivCount = 1;
    bytesToSend = m_writeIndex;
    return true;
}

//由线程池中的工作线程调用，这是处理http请求的入口函数
void http_conn::Process() {
    HTTP_CODE read_ret = ProcessRead();
    //如果请求不完整，继续读入客户端数据
    if(read_ret == NO_REQUEST) { 
        modfd(m_epollfd, m_socketfd, EPOLLIN, m_TRIGMode); //继续读
        return;
    }
    //解析请求
    bool write_ret = ProcessWrite(read_ret);
    if(!write_ret) {
        CloseConn();
    } 
    //需要发送数据，改为写状态
    // modfd(m_epollfd, m_socketfd, EPOLLOUT);
    modfd(m_epollfd, m_socketfd, EPOLLOUT, m_TRIGMode);
}

                                   