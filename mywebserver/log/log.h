#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "block_queue.h"

using namespace std;

#define LOG_DEBUG(format, ...) if(0 == m_closeLog) {Log::GetInstance()->WriteLog(0, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_INFO(format, ...) if(0 == m_closeLog) {Log::GetInstance()->WriteLog(1, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_WARN(format, ...) if(0 == m_closeLog) {Log::GetInstance()->WriteLog(2, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_ERROR(format, ...) if(0 == m_closeLog) {Log::GetInstance()->WriteLog(3, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}

class Log
{
public:
    //C++11以后,使用局部变量懒汉不用加锁
    static Log* GetInstance()
    {
        static Log instance;
        return &instance;
    }

    static void* FlushLogThread(void *args)
    {
        Log::GetInstance()->AsyncWriteLog();
    }

    //初始化，可选择的参数有日志文件，是否关闭日志，日志缓冲区大小，最大行数以及最长日志队列
    bool Init(const char *fileName, int closeLog, int logBufSize = 8192, int splitLines = 5000000, int maxQueueSize = 0);
    //写日志
    void WriteLog(int level, const char *format, ...);
    //刷新缓冲区
    void Flush(void);

private:
    Log();
    virtual ~Log();
    //异步写日志
    void* AsyncWriteLog()
    {
        string singleLog;
        //从阻塞队列中取出一个日志string，写入文件
        while (m_logQueue->Pop(singleLog))
        {
            m_mutex.Lock();
            if(m_level == 2 || m_level == 3) {
                fputs(singleLog.c_str(), m_errorFp);
            }
            else {
                fputs(singleLog.c_str(), m_fp);
            }
            m_mutex.Unlock();
        }
    }

private:
    char m_dirName[128];            //路径名
    char m_dirErrorName[128];       //错误日志路径名
    char m_logName[128];            //日志文件名
    char m_logErrorName[128];       //错误日志文件名
    int m_splitLines;               //日志最大行数
    int m_logBufSize;               //日志缓冲区大小
    long long m_count;              //日志行数记录
    int m_today;                    //因为按天分类,记录当前时间是那一天
    FILE *m_fp;                     //打开日志文件的指针
    FILE *m_errorFp;                //打开错误日志文件的指针
    char *m_buf;                    //日志缓冲区
    BlockQueue<string> *m_logQueue; //阻塞队列
    bool m_isAsync;                 //是否同步标志位
    Locker m_mutex;                 //互斥锁
    int m_closeLog;                 //关闭日志
    int m_level;                    //日志信息级别
};



#endif
