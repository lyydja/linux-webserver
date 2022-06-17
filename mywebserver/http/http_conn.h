#ifndef _HTTPCONNECTION_H_
#define _HTTPCONNECTION_H_

#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>  //va_list所需要的头文件
#include <sys/uio.h> //writev所需的头文件
#include <map>
#include <mysql/mysql.h>
#include "../lock/locker.h"
#include "../timer/timer.h"
#include "../log/log.h"
#include "../CGIMysql/sql_conntion_pool.h"



class http_conn
{
public:
    static const int FILENAME_LEN = 200; //文件名的最大长度
    static const int READ_BUF_SIZE = 2048; //读缓冲区的大小
    static const int WRITE_BUF_SIZE = 1024; //写缓冲区的大小
     //HTTP请求方法
    enum METHOD {GET = 0, POST, PUT, HEAD, DELETE, OPTIONS, TRACE, CONNECT, PATCH};
    //解析http请求时，主状态机所处的状态
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    //服务器处理http请求的可能结果，分别表示：0.请求不完整，需要继续读取客户数据、1.获得了一个完整的客户请求、2.请求有语法错误、3.无资源
    // 4.客户对资源没有足够的访问权限、5.文件获取成功、6.服务器内部错误、7.客户端已经关闭了连接
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSE_CONNECTION};
    //行的读取结果(从状态机的可能状态)，即行的读取状态，分别表示：读取到一个完整的行、行出错、行数据尚且不完整。行数据尚且不完整时继续读取
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN}; 

public:
    http_conn() {}
    ~http_conn() {}

    void Init(int socketfd, const sockaddr_in &addr, char *root, int TRIGMode,
              int close_log, string user, string passwd, string sqlname); //初始化新接受的请求
    void CloseConn(bool real_close = true);                               //关闭连接
    void Process();                                                       //处理客户请求
    bool Read();                                                          //无阻塞的读
    bool Write();                                                         //无阻塞的写
    sockaddr_in *GetAddress() { return &m_address; }                      //获取当前连接的地址
    void InitMysqlResult(ConnectionPool *connPool);                       //初始化数据库结果集

private:
    void Init();                                      //初始化连接

    /*读并且解析http请求函数*/
    HTTP_CODE ProcessRead();                            //解析http请求
    char *GetLine() { return m_readBuf + m_startLine; } //获取一行数据
    HTTP_CODE ParseRequestLine(char *text);             //解析请求行
    HTTP_CODE ParseHeaders(char *text);                 //解析头部信息
    HTTP_CODE ParseContent(char *text);                 //解析消息体
    LINE_STATUS ParseLine();                            //从状态机，用于解析一行内容
    HTTP_CODE DoRequest();                              //当得到一个正确的http请求时，我们就分析目标文件的属性

    /*http请求应答函数*/
    void UnMap();                                      //解除内存映射
    bool ProcessWrite(HTTP_CODE ret);                  //填充HTTP请求
    bool AddResponse(const char *format...);           //往写缓冲区中写入待发送的数据
    bool AddStatusLine(int status, const char *title); //添加状态行，响应报文的第一行
    bool AddContentLength(int content_len);            //添加内容长度：Content-Length: xxx
    bool AddLinger();                                  //添加是否保持连接：Connection: keep-alive / Connection: close
    bool AddBlankLine();                               //添加回车换行
    bool AddHeaders(int content_len);                  //添加头部信息（字段）
    bool AddContent(const char *content);              //添加消息体

public:
    //所有socket上的事件都被注册到同一个epoll内核事件中，所以将epoll文件描述符设置为静态的
    static int m_epollfd;
    static int m_userCount;
    int m_state;   //0为读，1为写
    int improv;    //Reactor模型下读写标志位， 1为有读写操作，0为没有读写操作
    int timerFlag; //定时器标志位，1为断开断开连接，同时清除定时器
    MYSQL *mysql;

private:
    /*该http连接的socket和对方的socket的地址*/
    int m_socketfd;
    sockaddr_in m_address;

    CHECK_STATE m_check_state; //主状态机目前所处的状态
    METHOD m_method;           //请求方法

    char *docRoot;                 //根目录
    char m_readBuf[READ_BUF_SIZE]; //读缓冲区
    int m_readIndex;               //下一次读入时，读缓冲区的起始位置
    int m_startLine;               //当前正在解析行的起始位置
    int m_checkIndex;              //当前正在分析的字符在缓冲区中的位置（即当前正在解析的内容在缓冲区中的起始位置）
    char *m_url;                   //客户请求的目标文件的文件名
    char *m_version;               // http协议版本号
    int m_contentLength;           // http请求的消息体的长度
    bool m_linger;                 // http请求是否要求保持连接
    char *m_host;                  //主机名
    bool m_printfFlag;             //打印标志位
    int cgi;                       //是否启用POST
    char *m_string;                //存储请求头数据

    char m_realFile[FILENAME_LEN];   //客户请求的目标文件的完整路径，其内容等于doc_root + m_url, doc_root时网站根目录
    struct stat m_fileStat;          //目标文件的状态。通过它我们可以判断文件是否存在、是否时目录、是否可读，并获取文件大小等信息
    char *m_fileAddress;             //客户请求的目标文件被mmap到内存中的位置
    struct iovec m_iv[2];            //我们将采用writev来执行写操作
    int m_ivCount;                   //背写内存块的数量
    char m_writeBuf[WRITE_BUF_SIZE]; //写缓冲区
    int m_writeIndex;                //写缓冲区中，待发送的字节数
    int bytesToSend;
    int bytesHaveSend;

    int m_TRIGMode; //触发模式
    int m_closeLog; //关闭日志

    char m_sqlUser[100];
    char m_sqlPasswd[100];
    char m_sqlName[100];
};

#endif