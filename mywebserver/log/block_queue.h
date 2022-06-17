/*************************************************************
*循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;  
*线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
**************************************************************/

#ifndef BLOCK_QUEUE1_H_
#define BLOCK_QUEUE1_H_

#include <iostream>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

//这个阻塞队列模块主要是解决异步写入日志。
template<typename T>
class BlockQueue {
private:
    Locker m_mutex;
    Cond m_cond;
    int m_size;
    int m_maxSize;
    T* m_array;
    int m_front;
    int m_back;
    
public:
    BlockQueue(int maxSize = 1000) {
        if(maxSize <= 0) {
            exit(-1);
        }

        m_maxSize = maxSize;
        m_size = 0;
    }
    ~BlockQueue() {
        m_mutex.Lock();
        if(m_array != nullptr) {
            delete[] m_array;
        }
        m_mutex.Unlock();
    }
    //判断队列是否满了
    bool Full() {
        m_mutex.Lock();
        if(m_size >= m_maxSize) {
            m_mutex.Unlock();
            return true;
        } 
        m_mutex.Unlock();
        return false;
    }
    //判断队列是否为空
    bool Empty() {
        m_mutex.Lock();
        if(0 == m_size) {
            m_mutex.Unlock();
            return true;
        }
        m_mutex.Unlock();
        return false;
    }
    //返回队首元素
    bool Front(T& value) {
        m_mutex.Lock();
        if(0 == m_size) {
            m_mutex.Unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.Unlock();
        return true;
    }
    //返回队尾元素
    bool Back(T& value) {
        m_mutex.Lock();
        if(0 == m_size) {
            m_mutex.Unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.Unlock();
        return true;
    }
    //往队列中添加元素，需要将所有使用队列的线程先唤醒，
    //当有元素添加进队列，相当于生产者生产了一个元素
    //若当前没有线程等待条件变量，则唤醒没有意义
    bool Push(const T& item) {
        m_mutex.Lock();
        if(m_size >= m_maxSize) {
            m_mutex.Unlock();
            return false;
        }
        m_back = (m_back + 1) % m_maxSize; //循环队列
        m_array[m_back] = item;
        m_size++;
        m_cond.BroadCast();
        m_mutex.Unlock();

        return true;
    }

    //pop时，如果当前队列没有元素，将会等待条件变量
    bool Pop(T& item) {
        m_mutex.Lock();
        while(m_size <= 0) {
            if(!m_cond.Wait(m_mutex.Get())) {
                m_mutex.Unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_maxSize;
        item = m_array[m_front];
        m_size--;
        m_mutex.Unlock();

        return true;
    }

    //增加了超时处理
    bool Pop(T& item, int ms_timeout) {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_mutex.Lock();
        if(m_size <= 0) {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout / 1000) * 1000;
            if(!m_cond.TimeWait(m_mutex.Get(), t)) {
                m_mutex.Unlock();
                return false;
            }
        }
        if(m_size <= 0) {
            m_mutex.Unlock();
            return false;
        }

        m_front = (m_front + 1) % m_maxSize;
        item = m_array[m_front];
        m_size--;
        m_mutex.Unlock();

        return true;
    }

    //清空队列
    void Clear() {
        m_mutex.Lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.Unlock();        
    }

    //返回队列的大小
    int Size() {
        int tmp = 0;
        m_mutex.Lock();
        tmp = m_size;
        m_mutex.Unlock();

        return tmp;
    }

    //返回队列可以添加元素的最大个数
    int MaxSize() {
        int tmp = 0;
        m_mutex.Lock();
        tmp = m_maxSize;
        m_mutex.Unlock();

        return tmp;
    }

};






#endif
