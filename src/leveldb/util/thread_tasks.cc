// -------------------------------------------------------------------
//
// thread_tasks.cc
//
// Copyright (c) 2015 Basho Technologies, Inc. All Rights Reserved.
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

#include "util/db_list.h"
#include "util/hot_threads.h"
#include "util/thread_tasks.h"

namespace leveldb {

void
CompactionTask::operator()()
{
    m_DBImpl->BackgroundCall2(m_Compaction);
    m_Compaction=NULL;

    // look for grooming compactions in other databases.
    // MUST submit to different pool, or will seldom work.
    if (0==gCompactionThreads->m_WorkQueueAtomic)
    {
        ThreadTask * task=new GroomingPollTask;

        // this sequence could be a race condition, and that is ok.
        // Race is when this thread is the grooming thread and
        // it deschedules for the entire time of the GroomingPollTasks'
        // scan.  oh well.  not critical.
        gWriteThreads->Submit(task, true);
    }   // if
}   // CompactionTask::operator()()


void
GroomingPollTask::operator()()
{
    // if there is no current backlog ... see if
    //  databases have grooming opportunity waiting
    // "false" only scan user databases, not internal
    if (0==gCompactionThreads->m_WorkQueueAtomic)
        DBList()->ScanDBs(false, &DBImpl::CheckAvailableCompactions);
    if (0==gCompactionThreads->m_WorkQueueAtomic)
        DBList()->ScanDBs(true, &DBImpl::CheckAvailableCompactions);

}   // GroomingPollTask::operator()

}  // namespace leveldb
