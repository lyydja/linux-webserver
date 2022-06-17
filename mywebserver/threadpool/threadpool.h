#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

//半同步/半反应堆线程池实现
//主线程往工作队列中插入任务，工作线程通过竞争来取得任务并执行它
#include <list>
#include <cstdio>
#include <pthread.h>
#include <exception>
#include "../lock/locker.h"
#include "../CGIMysql/sql_conntion_pool.h"

//线程池类，将它定义为模板类是为了代码复用。模板参数T是任务类
template<typename T>
class ThreadPool {
public:
    //参数thread_num是线程池中线程的数量， max_requestes是请求队列中最多允许的、等待处理的请求的数量
    ThreadPool(int actor_model, ConnectionPool *connPool, int thread_num = 8, int max_requests = 10000);
    ~ThreadPool();
    //往请求队列中添加任务
    bool Append(T *request, int state);
    bool AppendProactor(T* request);
    //工作线程运行的函数，它不断从工作队列中取出任务并执行
    static void* Worker(void* arg);
    void Run();

private:
    int m_thread_num;             //线程池中的线程数
    int m_max_requests;           //请求队列中最多允许的、等待处理的请求的数量
    pthread_t *m_threads;         //描述线程池的数组，其大小为m_thread_num;
    std::list<T *> m_workerqueue; //请求队列
    Locker m_queuelocker;         //保护请求队列的互斥锁
    Sem m_queuestat;              //是否有任务需要处理
    ConnectionPool* m_connPool;
    int m_actor_model;            //模型切换, 1为reactor模型， 0为模拟Proactor模型
};

template<typename T>
ThreadPool<T>::ThreadPool(int actor_model, ConnectionPool *connPool, int thread_num, int max_requests) 
    : m_thread_num(thread_num), m_max_requests(max_requests), m_threads(NULL), m_connPool(connPool)  {
    
    //如果给的线程数小于等于0，或者所允许的最大等待处理的请求的数量小于等于0
    if(thread_num <= 0 || max_requests <= 0) {
        throw std::exception();
    }
    //创建一个线程池数组，存放线程
    m_threads = new pthread_t[m_thread_num];
    if(!m_threads) {
        throw std::exception();
    }
    //创建thread_num个线程将其存放在数组中，并将他们都设置为脱离线程
    for(int i = 0; i < thread_num; i++) {
        // printf("create the %dth thread\n", i + 1);
        if(pthread_create(m_threads + i, NULL, Worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
        //设置线程分离
        if(pthread_detach(m_threads[i])) {
            //delete[] m_threads;
            throw std::exception();
        }
    }
}
template<typename T>
ThreadPool<T>::~ThreadPool() {
    delete[] m_threads;
}

//往请求队列中添加任务
template<typename T>
bool ThreadPool<T>::Append(T* request, int state) {
    //操作工作队列时一定要加锁，因为它被所有线程共享
    m_queuelocker.Lock();
    if(m_workerqueue.size() >= m_max_requests) {
        m_queuelocker.Unlock();
        return false;
    }
    request->m_state = state; // 判断读写位, 0为读，1为写 
    //添加任务
    m_workerqueue.push_back(request);
    m_queuelocker.Unlock();
    m_queuestat.Post();
    return true;
}

//往请求队列中添加任务
template<typename T>
bool ThreadPool<T>::AppendProactor(T* request) {
    //操作工作队列时一定要加锁，因为它被所有线程共享
    m_queuelocker.Lock();
    if(m_workerqueue.size() >= m_max_requests) {
        m_queuelocker.Unlock();
        return false;
    }
    //添加任务
    m_workerqueue.push_back(request);
    m_queuelocker.Unlock();
    m_queuestat.Post();
    return true;
}

//工作线程运行的函数，它不断从工作队列中取出任务并执行
//work和run都是处理请求任务的线程，可以想象为n个进程
template<typename T>
void* ThreadPool<T>::Worker(void* arg) {
    ThreadPool* pool = (ThreadPool*) arg;
    pool->Run();
    return pool;
}

template<typename T>
void ThreadPool<T>::Run() {
    //只有销毁线程的时候，该才会停止
    while(true) {
        m_queuestat.Wait();
        m_queuelocker.Lock();
        //如果工作队列为空，就是没有任务要处理
        if(m_workerqueue.empty()) {
            m_queuelocker.Unlock();
            continue;
        }
        //有任务要处理
        T* request = m_workerqueue.front();
        m_workerqueue.pop_front();
        m_queuelocker.Unlock();

        if(!request) {
            continue;
        }
        if(1 == m_actor_model) {
            //读
            if(0 == request->m_state) {
                if(request->Read()) {
                    //正确读取数据
                    request->improv = 1;
                    //取到了任务，去处理
                    request->Process();
                }
                else {
                    //读取数据失败，断开连接
                    request->improv = 1;
                    ConnectionRAII mysqlConn(&request->mysql, m_connPool);
                    request->timerFlag = 1;
                }
            } 
            //写
            else {
                if(request->Write()) {
                    //正确回应了响应
                    request->improv = 1;
                }
                else {
                    //写响应信息失败
                    request->improv = 1;
                    request->timerFlag = 1;
                }
            }          
        }
        else {
            ConnectionRAII mysqlConn(&request->mysql, m_connPool);
            request->Process();
        }
    }
}


#endif