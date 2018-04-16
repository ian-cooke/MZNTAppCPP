#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <pthread.h>
#include <iostream>
#include <assert.h>
#include "ThreadSafeQueue.h"

using namespace std;

//----------------------------------------------------------------------
// This is a wrapper around our actual pthread
//----------------------------------------------------------------------
class ThreadInstance
{
public:
    ThreadInstance()
    {
        m_state = state_none;
        m_thread = 0;
    }
    virtual ~ThreadInstance()
    {
        assert(m_state == state_joined);
    }

    void Start()
    {
        if(pthread_create(&m_thread,NULL,StartHelper,this))
        {
            return;
        }
        m_state = state_started;
    }

    void Join()
    {
        pthread_join(m_thread, NULL);
        m_state = state_joined;
    }

protected:
    virtual void Run() = 0;

private:
    static void *StartHelper( void *inst ){
        ThreadInstance *ti = (ThreadInstance*)inst;
        ti->Run();
        return NULL;
    }
    pthread_t m_thread;    
    enum state {
        state_none,
        state_joined,
        state_started
    };
    ThreadInstance::state m_state;
};

//----------------------------------------------------------------------
// Abstact "task" to be defined
//----------------------------------------------------------------------
class Task
{
public:
    Task() {}
    virtual ~Task() {}
    virtual void Run() = 0;
    virtual void ShowTask() = 0;
};


//----------------------------------------------------------------------
// PoolThread
// Requires: work_queue
// Actions: continuously requests tasks and executes them.
//----------------------------------------------------------------------
class PoolThread : public ThreadInstance
{
public:
    PoolThread(ThreadSafeQueue<Task*> &work_queue ) : m_work_queue(work_queue)
    {
        m_busy = false;
    }
    bool GetBusyStatus()
    {
        return m_busy;
    }

protected:
    virtual void Run()
    {
        while(Task *task = m_work_queue.pop_front())
        {
            cout << "Thread got task." << endl;
            m_busy = true;
            if(task == NULL){
                break;
            }
            task->Run();
            m_busy = false;
        }
    }
private:
    bool m_busy;
    ThreadSafeQueue<Task*>& m_work_queue;
};

//----------------------------------------------------------------------
// Contains a pool of threads and a queue of tasks
//----------------------------------------------------------------------
class ThreadPool
{
public:
    ThreadPool( int size ) : m_taskQueue(size*2)
    {
        cout << "Creating thread pool of size " << dec << size << endl;
        for(int i =0; i< size; i++)
        {
            m_pool.push_back(new PoolThread(m_taskQueue));
            m_pool.back()->Start(); // Start all the workers
        }
        cout << "Thread Pool started all workers." << endl;
    }
    ~ThreadPool()
    {
        cout << "Thread pool waiting for workers..." << endl;
        // Add null task to signal them to stop.
        int poolSize = m_pool.size();
        for(int i=0; i < poolSize; ++i) {
            m_taskQueue.push(NULL);
        }
        for(int i=0; i < poolSize; ++i) {
            m_pool[i]->Join();
            delete m_pool[i];
        }
        m_pool.clear();
        cout << "Thread pool exiting" << endl;
    }

    void AddTask(Task *task) {
        m_taskQueue.push(task);
    }

    bool AllThreadsOccupied()
    {
        for( unsigned int i=0; i <m_pool.size(); i++)
        {
            if(!m_pool[i]->GetBusyStatus())
                return false;
        }

        return false;
    }

    unsigned int GetNumTasksRemain(){
        return m_taskQueue.size();
    }

private:
    std::vector<PoolThread*> m_pool;
    ThreadSafeQueue<Task*> m_taskQueue;
    unsigned int m_numTasks;
};


#endif