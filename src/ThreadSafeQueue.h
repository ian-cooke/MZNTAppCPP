/*
 * ThreadSafeQueue.h
 *
 *  Created on: Mar 14, 2018
 *      Author: chris
 */

#ifndef SRC_THREADSAFEQUEUE_H_
#define SRC_THREADSAFEQUEUE_H_

#include <pthread.h>   
#include <queue>

template <class T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue(unsigned int size);
    ~ThreadSafeQueue();

    bool push(T value);

    T pop_front();

    unsigned int size() {
        pthread_mutex_lock(&m_lock);
        unsigned int retValue =  m_tsQueue.size();
        pthread_mutex_unlock(&m_lock);
        return retValue;
    }

private:
    pthread_mutex_t m_lock;
    pthread_cond_t m_condc;
    pthread_cond_t m_condp;
    std::queue <T>m_tsQueue;
    unsigned int m_maxSize;
};

#endif