// -------------------------------------------------------------------
//
// hot_threads.h
//
// Copyright (c) 2011-2015 Basho Technologies, Inc. All Rights Reserved.
//
// This file is provided to you under the Apache License,
// Version 2.0 (the "License"); you may not use this file
// except in compliance with the License.  You may obtain
// a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//
// -------------------------------------------------------------------

// -------------------------------------------------------------------
// HotThread is a subtle variation on the eleveldb_thread_pool.  Both
//  represent a design pattern that is tested to perform better under
//  the Erlang VM than other traditional designs.
// -------------------------------------------------------------------

#ifndef STORAGE_LEVELDB_INCLUDE_HOT_THREADS_H_
#define STORAGE_LEVELDB_INCLUDE_HOT_THREADS_H_

#include <pthread.h>
#include <semaphore.h>
#include <deque>
#include <vector>

#include "leveldb/perf_count.h"
#include "port/port.h"
#include "util/mutexlock.h"

namespace leveldb
{

// forward declare
class ThreadTask;

/**
 * Meta / managment data related to a worker thread.
 */
struct HotThread
{
public:
    pthread_t m_ThreadId;                //!< handle for this thread

    volatile uint32_t m_Available;       //!< 1 if thread waiting, using standard type for atomic operation
    class HotThreadPool & m_Pool;        //!< parent pool object
    volatile ThreadTask * m_DirectWork;  //!< work passed direct to thread
    int m_Nice;                          //!< amount to adjust sched priority

    port::Mutex m_Mutex;             //!< mutex for condition variable
    port::CondVar m_Condition;          //!< condition for thread waiting

public:
    HotThread(class HotThreadPool & Pool, int Nice)
    : m_Available(0), m_Pool(Pool), m_DirectWork(NULL), m_Nice(Nice),
        m_Condition(&m_Mutex)
    {}   // HotThread

    virtual ~HotThread() {};

    // actual work loop
    void * ThreadRoutine();

private:
    HotThread();                              // no default
    HotThread(const HotThread &);             // no copy
    HotThread & operator=(const HotThread&);  // no assign

};  // class HotThread


class HotThreadPool
{
public:
    std::string m_PoolName;              //!< used to name threads for gdb / core
    typedef std::deque<ThreadTask*> WorkQueue_t;
    typedef std::vector<HotThread *>   ThreadPool_t;

    volatile bool m_Shutdown;            //!< should we stop threads and shut down?

    ThreadPool_t  m_Threads;             //!< pool of fast response workers

    WorkQueue_t   m_WorkQueue;
    port::Spin m_QueueLock;              //!< protects access to work_queue
    volatile size_t m_WorkQueueAtomic;   //!< atomic size to parallel work_queue.size().

    enum PerformanceCountersEnum m_DirectCounter;
    enum PerformanceCountersEnum m_QueuedCounter;
    enum PerformanceCountersEnum m_DequeuedCounter;
    enum PerformanceCountersEnum m_WeightedCounter;

public:
    HotThreadPool(const size_t thread_pool_size, const char * Name,
                  enum PerformanceCountersEnum Direct,
                  enum PerformanceCountersEnum Queued,
                  enum PerformanceCountersEnum Dequeued,
                  enum PerformanceCountersEnum Weighted,
                  int Nice=0);

    virtual ~HotThreadPool();

    static void *ThreadStart(void *args);

    bool FindWaitingThread(ThreadTask * work, bool OkToQueue=true);

    bool Submit(ThreadTask * item, bool OkToQueue=true);

    size_t work_queue_size() const { return m_WorkQueue.size();}
    bool shutdown_pending() const  { return m_Shutdown; }
    leveldb::PerformanceCounters * perf() const {return(leveldb::gPerfCounters);};

    void IncWorkDirect() {leveldb::gPerfCounters->Inc(m_DirectCounter);};
    void IncWorkQueued() {leveldb::gPerfCounters->Inc(m_QueuedCounter);};
    void IncWorkDequeued() {leveldb::gPerfCounters->Inc(m_DequeuedCounter);};
    void IncWorkWeighted(uint64_t Count) {leveldb::gPerfCounters->Add(m_WeightedCounter, Count);};

private:
    HotThreadPool(const HotThreadPool &);             // nocopy
    HotThreadPool& operator=(const HotThreadPool&);  // nocopyassign

};  // class HotThreadPool

extern HotThreadPool * gImmThreads;
extern HotThreadPool * gWriteThreads;
extern HotThreadPool * gLevel0Threads;
extern HotThreadPool * gCompactionThreads;

} // namespace leveldb


#endif  // STORAGE_LEVELDB_INCLUDE_HOT_THREADS_H_
