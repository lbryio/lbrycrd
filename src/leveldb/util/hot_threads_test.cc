// -------------------------------------------------------------------
//
// hot_threads_test.cc
//
// Copyright (c) 2016 Basho Technologies, Inc. All Rights Reserved.
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


#include "util/testharness.h"
#include "util/testutil.h"

#include "port/port.h"
#include "util/hot_threads.h"
#include "util/mutexlock.h"
#include "util/thread_tasks.h"

/**
 * Execution routine
 */
int main(int argc, char** argv)
{
  return leveldb::test::RunAllTests();
}


namespace leveldb {

// helper function to clean up heap objects
static void ClearMetaArray(Version::FileMetaDataVector_t & ClearMe);


/**
 * Wrapper class for tests.  Holds working variables
 * and helper functions.
 */
class HotThreadsTester
{
public:
    HotThreadsTester()
    {
    };

    ~HotThreadsTester()
    {
    };
};  // class HotThreadsTester


class RaceTask : public ThreadTask
{
public:
    port::Mutex * m_Mutex;
    port::CondVar * m_Condition;
    volatile bool * m_ReadyFlag;

    RaceTask() {};
    virtual ~RaceTask() {};

    virtual void operator()()
    {
        volatile bool flag;

        // is other thread waiting yet
        do
        {
            MutexLock lock(m_Mutex);
            flag=*m_ReadyFlag;
        } while(!flag);

        {
            MutexLock lock(m_Mutex);
            *m_ReadyFlag=false;
            m_Condition->SignalAll();
        }
    };  // operator()

};  // class RaceTask

/**
 * Reproduce race condition where all threads go to
 *  into m_Condition.Wait() without seeing new work item
 *  on queue (valgrind helps make failed code fail).
 */
TEST(HotThreadsTester, RaceCondition)
{
    HotThreadPool pool(1, "RacePool", ePerfDebug0,ePerfDebug1,ePerfDebug2,ePerfDebug3);
    port::Mutex race_mutex;
    port::CondVar race_condition(&race_mutex);
    int loop_count(0);
    volatile bool ready_flag;
    int loop;
    RaceTask * task;

    for (loop=0; loop<10000000; ++loop)
    {
        task=new RaceTask;
        task->m_Mutex=&race_mutex;
        task->m_Condition=&race_condition;
        task->m_ReadyFlag=&ready_flag;

        ready_flag=false;
        pool.Submit(task,true);

        {
            MutexLock lock(&race_mutex);
            ready_flag=true;
            race_condition.Wait();
        }
    }   // for

    printf("loop: %d\n",loop);
}   // test




}  // namespace leveldb

