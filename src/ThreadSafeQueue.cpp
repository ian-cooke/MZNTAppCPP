/*
 * ThreadSafeQueue.cpp
 *
 *  Created on: Mar 14, 2018
 *      Author: chris
 */
#include "ThreadSafeQueue.h"
#include <iostream>

using namespace std;

ThreadSafeQueue::ThreadSafeQueue(unsigned int size) {
    m_maxSize = size;

    pthread_mutex_init(&m_lock, NULL);

    pthread_cond_init(&m_condc, NULL);

    pthread_cond_init(&m_condp,NULL);
}

ThreadSafeQueue::~ThreadSafeQueue() {
    pthread_mutex_destroy(&m_lock);

    pthread_cond_destroy( &m_condc);
    pthread_cond_destroy( &m_condp);
}

bool ThreadSafeQueue::push(char *value) 
{
    // Acquire lock
    pthread_mutex_lock( &m_lock );

    // This puts blocks calling thread if array is full.
    while( m_tsQueue.size() == m_maxSize ) {
        cerr << "TS Queue Full, sleeping." << endl;
        pthread_cond_wait( &m_condp, &m_lock );
    }

    // Push the item.
    m_tsQueue.push(value);

    pthread_mutex_unlock( &m_lock );

    pthread_cond_signal(&m_condc);

    return true;
}

char *ThreadSafeQueue::pop_front() {
    pthread_mutex_lock(&m_lock);

    while(m_tsQueue.empty()) {
        //cerr << "TSQueue empty, sleeping." << endl;
        pthread_cond_wait(&m_condc,&m_lock);
    }

    char *retValue = m_tsQueue.front();
    m_tsQueue.pop();

    pthread_mutex_unlock(&m_lock);

    pthread_cond_signal(&m_condp);

    return retValue;
}