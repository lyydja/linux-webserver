#include "log.h"

Log::Log()
{
    m_count = 0;
    m_isAsync = false;
}

Log::~Log()
{
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
    if (m_errorFp != NULL)
    {
        fclose(m_errorFp);
    }
}
//异步需要设置阻塞队列的长度，同步不需要设置
//把错误警告的信息和其他信息分开了，方便查看，程序的写法有点不简略，有待于优化
bool Log::Init(const char *fileName, int closeLog, int logBufSize, int splitLines, int maxQueueSize)
{
    //如果设置了maxQueueSize,则设置为异步
    if (maxQueueSize >= 1)
    {
        m_isAsync = true;
        m_logQueue = new BlockQueue<string>(maxQueueSize);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, FlushLogThread, NULL);
    }
    
    m_closeLog = closeLog;
    m_logBufSize = logBufSize;
    m_buf = new char[m_logBufSize];
    memset(m_buf, '\0', m_logBufSize);
    m_splitLines = splitLines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

 
    const char *p = strrchr(fileName, '/');//获取左后一个'/'的位置，例如：./var/ServerLog，p指向S前面的'/'
    const char *p_error = strrchr(fileName, '/');
    char log_full_name[300] = {0}; //数组的大小根据实际来改吧
    char log_error_full_name[300] = {0};

    //没有'/'的情况    例如：fileName = ServerLog
    if (p == NULL)
    {
        snprintf(log_full_name, 300, "%04d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, fileName);
    }
    //有'/'的情况   例如：fileName = ./ServerLog
    else
    {
        strcpy(m_logName, p + 1); //m_logName = ServerLog
        strncpy(m_dirName, fileName, p - fileName + 1); //m_dirName = ./var
        snprintf(log_full_name, 300, "%s%04d_%02d_%02d_%s", m_dirName, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_logName);   
    }

    if(p_error == NULL) {
        snprintf(log_error_full_name, 300, "%04d_%02d_%02d_error_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, fileName);
    }
    else {
        strcpy(m_logErrorName, p_error + 1); //m_logErrorName = ServerLog
        strncpy(m_dirErrorName, fileName, p_error - fileName + 1); //m_dirErrorName = ./var
        snprintf(log_error_full_name, 300, "%s%04d_%02d_%02d_error_%s", m_dirErrorName, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_logErrorName);
    }

    m_today = my_tm.tm_mday;
    
    //文件指针指向文件
    m_fp = fopen(log_full_name, "a");
    m_errorFp = fopen(log_error_full_name, "a");

    if (m_fp == NULL) 
    {
        cout << "m_fp 打开失败" << endl;
        return false;
    }
    if(m_errorFp == NULL) {
        cout << "m_errorFp 打开失败" << endl;
        return false;
    }

    return true;
}

void Log::WriteLog(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0}; //记录错误类别
    m_level = level; //记录错误级别，用于判断是 错误和警告信息 还是 其他信息
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    m_mutex.Lock();
    //写入一个log，对m_count++, m_splitLines最大行数
    m_count++;

    //如果日期不同或者超过了最大行数，创建一个新的日志文件
    if (m_today != my_tm.tm_mday || m_count % m_splitLines == 0) //everyday log
    {
        
        char new_log[300] = {0};
        char new_error_log[300] = {0};
        if(m_level == 2 || m_level == 3) {
            fflush(m_errorFp);
            fclose(m_errorFp);
        }
        else {
            fflush(m_fp);
            fclose(m_fp);
        }
        char tail[16] = {0};
        char error_tail[21] = {0};
       
        if(level == 2 || level == 3) {
            snprintf(tail, 16, "%04d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        }
        else {
            snprintf(error_tail, 21, "%04d_%02d_%02d_error_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        }
        
        if (m_today != my_tm.tm_mday)
        {   
            if(level == 2 || level == 3) {
                snprintf(new_error_log, 300, "%s%s%s", m_dirErrorName, error_tail, m_logErrorName);
            }
            else {
                snprintf(new_log, 300, "%s%s%s", m_dirName, tail, m_logName);
            }
            
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {   
            if(level == 2 || level == 3) {
                snprintf(new_error_log, 300, "%s%s%s.%lld", m_dirErrorName, error_tail, m_logErrorName, m_count / m_splitLines);
            }
            else {
                snprintf(new_log, 300, "%s%s%s.%lld", m_dirName, tail, m_logName, m_count / m_splitLines);
            }
        }
        //如果是错误或警告，放到error文件中
        if(level == 2 || level == 3) {
            m_errorFp = fopen(new_error_log, "a");
        }
        //如果是其他信息，放到正常的文件中
        else {
            m_fp = fopen(new_log, "a");
        }
        
    }
 
    m_mutex.Unlock();

    va_list valst;
    va_start(valst, format);

    string logStr;
    m_mutex.Lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    
    int m = vsnprintf(m_buf + n, m_logBufSize - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    logStr = m_buf;

    m_mutex.Unlock();

    if (m_isAsync && !m_logQueue->Full())
    {
        m_logQueue->Push(logStr);
    }
    else
    {
        m_mutex.Lock();
        if(level == 2 || level == 3) {
            fputs(logStr.c_str(), m_errorFp);
        }
        else {
            fputs(logStr.c_str(), m_fp);
        }
        m_mutex.Unlock();
    }

    va_end(valst);
}

void Log::Flush(void)
{
    m_mutex.Lock();
    //强制刷新写入流缓冲区
    if(m_level == 2 || m_level == 3) {
        fflush(m_errorFp);
    }
    else {
        fflush(m_fp);
    }
    
    m_mutex.Unlock();
}
