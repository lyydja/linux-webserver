//主程序
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <cassert>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <exception>
#include "./http/http_conn.h"
#include "./lock/locker.h"
#include "./threadpool/threadpool.h"
#include "./log/log.h"
#include "./webserver/webserver.h"
#include "./config/config.h"

int main(int argc, char* argv[]) {

    //需要修改的数据库信息：登录名，密码，库名
    string user = "root";
    string passwd = "123456";
    string databasename = "Webserver";

    //命令行解析
    Config config;
    config.ParseArg(argc, argv);

    WebServer webserver;

    //初始化
    webserver.Init(config.PORT, user, passwd, databasename ,config.LOGWRITE, 
    config.THREADNUM, config.OPTLINGER, config.TRIGMODE, config.SQLNUM, 
    config.CLOSELOG, config.ACTORMODEL);
    //开启日志
    webserver.LogStart();
    //数据库
    webserver.SqlPool();
    //创建线程池
    webserver.Threadpool();
    //监听客户端的连接
    webserver.EventListen();

    webserver.EventLoop();

    return 0;
}