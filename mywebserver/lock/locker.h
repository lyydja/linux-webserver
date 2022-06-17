#ifndef _LOCKER_H_
#define _LOCKER_H_

//线程同步机制包装类

#include <pthread.h>
#include <exception>
#include <semaphore.h>

//互斥锁的类
class Locker {
public:
    //创建并初始化互斥锁
    Locker() {
        //如果创建并初始化不成功
        if(pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }
    //销毁互斥锁      
    ~Locker() {
        pthread_mutex_destroy(&m_mutex);
    }
    //获取互斥锁     
    bool Lock() {
        return pthread_mutex_lock(& m_mutex) == 0;
    } 
    //释放互斥锁  
    bool Unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    } 
    
    pthread_mutex_t *Get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex; 
};

//信号量的类
class Sem {
public:
    //创建并初始化信号量
    Sem() {
        if(sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }
    Sem(int num) {
        if(sem_init(&m_sem, 0, num) != 0) {
            throw std::exception();
        }
    }
    //销毁信号量
    ~Sem() {
        sem_destroy(&m_sem);
    }
    //等待信号量
    bool Wait() {
        return sem_wait(&m_sem) == 0;
    }
    //增加信号量
    bool Post() {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;

};

//封装条件变量的类
class Cond {
public:
    //创建并初始化条件变量
    Cond() {
        if(pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }
    //销毁条件变量
    ~Cond() {
        pthread_cond_destroy(&m_cond);
    }
    //等待条件变量
    bool Wait(pthread_mutex_t * mutex) {
        int ret = 0;
        pthread_mutex_lock(mutex);
        ret = pthread_cond_wait(&m_cond, mutex);
        pthread_mutex_unlock(mutex);
        return ret == 0;
    }
    bool TimeWait(pthread_mutex_t * mutex, struct timespec t) {
        int ret = 0;
        pthread_mutex_lock(mutex);
        ret = pthread_cond_timedwait(&m_cond, mutex, &t);
        pthread_mutex_unlock(mutex);
        return ret == 0;
    }
    //唤醒等待条件变量的线程
    bool Signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool BroadCast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
    
private:
    pthread_cond_t m_cond;

};




#endif