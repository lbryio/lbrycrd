// -------------------------------------------------------------------
//
// hot_threads.cc
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

#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "leveldb/atomics.h"
#include "util/hot_threads.h"
#include "util/thread_tasks.h"

namespace leveldb {

HotThreadPool * gImmThreads=NULL;
HotThreadPool * gWriteThreads=NULL;
HotThreadPool * gLevel0Threads=NULL;
HotThreadPool * gCompactionThreads=NULL;



void *ThreadStaticEntry(void *args)
{
    HotThread &tdata = *(HotThread *)args;

    return(tdata.ThreadRoutine());

}   // ThreadStaticEntry


/**
 * Worker threads:  worker threads have 3 states:
 *  A. doing nothing, available to be claimed: m_Available=1
 *  B. processing work passed by Erlang thread: m_Available=0, m_DirectWork=<non-null>
 *  C. processing backlog queue of work: m_Available=0, m_DirectWork=NULL
 */
void *
HotThread::ThreadRoutine()
{
    ThreadTask * submission;

    submission=NULL;

    port::SetCurrentThreadName(m_Pool.m_PoolName.c_str());
#ifdef OS_LINUX
    if (0!=m_Nice)
    {
        pid_t tid;
        int ret_val;

        tid = syscall(SYS_gettid);
        if (-1!=(int)tid)
        {
            errno=0;
            ret_val=getpriority(PRIO_PROCESS, tid);
            // ret_val could be -1 legally, so double test
            if (-1!=ret_val || 0==errno)
                setpriority(PRIO_PROCESS, tid, ret_val+m_Nice);

            assert((ret_val+m_Nice)==getpriority(PRIO_PROCESS, tid));
        }   // if
    }   // if
#endif
    while(!m_Pool.m_Shutdown)
    {
        // is work assigned yet?
        //  check backlog work queue if not
        if (NULL==submission)
        {
            // test non-blocking size for hint (much faster)
            if (0!=m_Pool.m_WorkQueueAtomic)
            {
                // retest with locking
                SpinLock lock(&m_Pool.m_QueueLock);

                if (!m_Pool.m_WorkQueue.empty())
                {
                    submission=m_Pool.m_WorkQueue.front();
                    m_Pool.m_WorkQueue.pop_front();
                    dec_and_fetch(&m_Pool.m_WorkQueueAtomic);
                    m_Pool.IncWorkDequeued();
                    m_Pool.IncWorkWeighted(Env::Default()->NowMicros()
                                           - submission->m_QueueStart);
                }   // if
            }   // if
        }   // if


        // a work item identified (direct or queue), work it!
        //  then loop to test queue again
        if (NULL!=submission)
        {
            // execute the job
            (*submission)();
            if (submission->resubmit())
            {
                submission->recycle();
                m_Pool.Submit(submission);
            }

            submission->RefDec();

            submission=NULL;
        }   // if

        // no work found, attempt to go into wait state
        //  (but retest queue before sleep due to race condition)
        else
        {
            MutexLock lock(&m_Mutex);

            m_DirectWork=NULL; // safety

            // only wait if we are really sure no work pending
            if (0==m_Pool.m_WorkQueueAtomic)
            {
                // yes, thread going to wait. set available now.
                m_Available=1;
                m_Condition.Wait();
            }    // if

            m_Available=0;    // safety
            submission=(ThreadTask *)m_DirectWork; // NULL is valid
            m_DirectWork=NULL;// safety
        }   // else
    }   // while

    return 0;

}   // HotThread::ThreadRoutine




HotThreadPool::HotThreadPool(
    const size_t PoolSize,
    const char * Name,
    enum PerformanceCountersEnum Direct,
    enum PerformanceCountersEnum Queued,
    enum PerformanceCountersEnum Dequeued,
    enum PerformanceCountersEnum Weighted,
    int Nice)
    : m_PoolName((Name?Name:"")),    // this crashes if Name is NULL ...but need it set now
      m_Shutdown(false),
      m_WorkQueueAtomic(0),
      m_DirectCounter(Direct), m_QueuedCounter(Queued),
      m_DequeuedCounter(Dequeued), m_WeightedCounter(Weighted)
{
    int ret_val;
    size_t loop;
    HotThread * hot_ptr;

    ret_val=0;
    for (loop=0; loop<PoolSize && 0==ret_val; ++loop)
    {
        hot_ptr=new HotThread(*this, Nice);

        ret_val=pthread_create(&hot_ptr->m_ThreadId, NULL,  &ThreadStaticEntry, hot_ptr);
        if (0==ret_val)
            m_Threads.push_back(hot_ptr);
        else
            delete hot_ptr;
    }   // for

    m_Shutdown=(0!=ret_val);

    return;

}   // HotThreadPool::HotThreadPool


HotThreadPool::~HotThreadPool()
{
    ThreadPool_t::iterator thread_it;
    WorkQueue_t::iterator work_it;
    // set flag
    m_Shutdown=true;

    // get all threads stopped
    for (thread_it=m_Threads.begin(); m_Threads.end()!=thread_it; ++thread_it)
    {
        {
            MutexLock lock(&(*thread_it)->m_Mutex);
            (*thread_it)->m_Condition.SignalAll();
        }   // lock

        pthread_join((*thread_it)->m_ThreadId, NULL);
        delete *thread_it;
    }   // for

    // release any objects hanging in work queue
    for (work_it=m_WorkQueue.begin(); m_WorkQueue.end()!=work_it; ++work_it)
    {
        (*work_it)->RefDec();
    }   // for

    return;

}   // HotThreadPool::~HotThreadPool


bool                           // returns true if available worker thread found and claimed
HotThreadPool::FindWaitingThread(
    ThreadTask * work, // non-NULL to pass current work directly to a thread,
                       // NULL to potentially nudge an available worker toward backlog queue
    bool OkToQueue)
{
    bool ret_flag;
    size_t start, index, pool_size;

    ret_flag=false;

    // pick "random" place in thread list.  hopefully
    //  list size is prime number.
    pool_size=m_Threads.size();
    if (OkToQueue)
        start=(size_t)pthread_self() % pool_size;
    else
        start=0;
    index=start;

    do
    {
        // perform quick test to see thread available
        if (0!=m_Threads[index]->m_Available && !shutdown_pending())
        {
            // perform expensive compare and swap to potentially
            //  claim worker thread (this is an exclusive claim to the worker)
            ret_flag = compare_and_swap(&m_Threads[index]->m_Available, 1, 0);

            // the compare/swap only succeeds if worker thread is sitting on
            //  pthread_cond_wait ... or is about to be there but is holding
            //  the mutex already
            if (ret_flag)
            {

                // man page says mutex lock optional, experience in
                //  this code says it is not.  using broadcast instead
                //  of signal to cover one other race condition
                //  that should never happen with single thread waiting.
                MutexLock lock(&m_Threads[index]->m_Mutex);
                m_Threads[index]->m_DirectWork=work;
                m_Threads[index]->m_Condition.SignalAll();
            }   // if
        }   // if

        index=(index+1)%pool_size;

    } while(index!=start && !ret_flag && OkToQueue);

    return(ret_flag);

}   // FindWaitingThread


bool
HotThreadPool::Submit(
    ThreadTask* item,
    bool OkToQueue)
{
    bool ret_flag(false);

    if (NULL!=item)
    {
        item->RefInc();

        // do nothing if shutting down
        if(shutdown_pending())
        {
            item->RefDec();
            ret_flag=false;
        }   // if

        // try to give work to a waiting thread first
        else if (FindWaitingThread(item, OkToQueue))
        {
            IncWorkDirect();
            ret_flag=true;
        }   // else if

        else if (OkToQueue)
        {
            // hold mutex of only thread 0, this synchronizes this
            //  thread and the first worker thread to ensure at least
            //  one thread will eventually see the work item on the queue
            //  before m_Condition.Wait()
            {
                item->m_QueueStart=Env::Default()->NowMicros();

                MutexLock lock(&m_Threads[0]->m_Mutex);

                // no waiting threads, put on backlog queue
                {
                    SpinLock lock(&m_QueueLock);
                    inc_and_fetch(&m_WorkQueueAtomic);
                    m_WorkQueue.push_back(item);
                }
            }   // mutex released

            // to address race condition, thread might be waiting now
            FindWaitingThread(NULL, true);

            IncWorkQueued();
            ret_flag=true;
        }   // else if

        // did not post to thread or queue
        else
        {
            item->RefDec();
            ret_flag=false;  // redundant, but safe
        }   // else
    }   // if

    return(ret_flag);

}   // HotThreadPool::Submit

};  // namespace leveldb
