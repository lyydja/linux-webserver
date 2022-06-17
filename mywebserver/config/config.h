#ifndef CONFIG_H_
#define CONFIG_H_

#include <iostream>
#include <stdio.h>
#include <string>
#include <unistd.h>

//解析main函数的输入，配置相关参数
class Config {
public:
    Config();
    ~Config() {}
    void ParseArg(int argc, char* argv[]); //接收main 函数的输入，将成员变量设置成对应的值
    
public:
    int PORT;           //服务器的端口号
    int LOGWRITE;       //日志写入方式
    int TRIGMODE;       //触发组合模式
    int LISTENTRIGMODE; //listenfd触发模式
    int CONNTRIGMODE;   //conn触发模式
    int OPTLINGER;      //优雅的关闭连接
    int SQLNUM;         //数据库数量
    int THREADNUM;      //线程池内的线程数量
    int CLOSELOG;       //是否关闭日志
    int ACTORMODEL;     //并发模型选择
};





#endif