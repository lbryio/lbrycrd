// testharness used is 
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

// -------------------------------------------------------------------
//
// perf_count_test.cc:  unit tests for LevelDB performance counters
//
// Copyright (c) 2012-2013 Basho Technologies, Inc. All Rights Reserved.
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

#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "leveldb/perf_count.h"
#include "util/testharness.h"

namespace leveldb {

class PerfTest 
{
public:
    static PerfTest* current_;

    PerfTest()
    {
        current_ = this;
    }

    ~PerfTest() {};

    bool
    DeleteShm(key_t Key)
    {
        int ret_val, id;

        id=shmget(Key, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (-1!=id)
            ret_val=shmctl(id, IPC_RMID, NULL);
        else
            ret_val=-1;

        return(0==ret_val);
    }


    bool
    CreateShm(key_t Key, size_t Size)
    {
        int ret_val;

        ret_val=shmget(Key, Size, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        return(-1!=ret_val);
    }


    void *
    MapShm(key_t Key)
    {
        int id;
        void * ret_ptr;

        id=shmget(Key, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (-1!=id)
            ret_ptr=shmat(id, NULL, 0);
        else
            ret_ptr=NULL;

        return(ret_ptr);
    }



    size_t
    GetShmSize(key_t Key)
    {
        int ret_val, id;
        struct shmid_ds shm_info;

        memset(&shm_info, 0, sizeof(shm_info));
        id=shmget(Key, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (-1!=id)
        {
            ret_val=shmctl(id, IPC_STAT, &shm_info);

            if (0!=ret_val)
                shm_info.shm_segsz=0;
        }   // if
        return(shm_info.shm_segsz);
    }

};  // class PerfTest


PerfTest* PerfTest::current_;


TEST(PerfTest, CreateNew) 
{
    PerformanceCounters * perf_ptr;

    // clear any existing shm
    DeleteShm(ePerfKey);

    // open for write, will create
    perf_ptr=PerformanceCounters::Init(false);
    ASSERT_NE(perf_ptr, (void*)NULL);
    ASSERT_EQ(sizeof(PerformanceCounters), GetShmSize(ePerfKey));

    // close and reopen for read
    ASSERT_EQ(0, PerformanceCounters::Close(perf_ptr));
    
    perf_ptr=PerformanceCounters::Init(true);
    ASSERT_NE(perf_ptr, (void*)NULL);
    ASSERT_EQ(sizeof(PerformanceCounters), GetShmSize(ePerfKey));
    ASSERT_EQ(0, PerformanceCounters::Close(perf_ptr));

    // cleanup
    ASSERT_EQ(true, DeleteShm(ePerfKey));

    return;

}   // CreateNew


TEST(PerfTest, SizeUpgrade)
{
    PerformanceCounters * perf_ptr;

    // clear any existing shm
    DeleteShm(ePerfKey);

    // Riak 1.2 was 536 bytes
    ASSERT_NE(536, sizeof(PerformanceCounters));
    ASSERT_EQ(true, CreateShm(ePerfKey, 536));
    ASSERT_EQ(536, GetShmSize(ePerfKey));

    // open for write, will recreate to current size
    perf_ptr=PerformanceCounters::Init(false);
    ASSERT_NE(perf_ptr, (void*)NULL);
    ASSERT_EQ(sizeof(PerformanceCounters), GetShmSize(ePerfKey));

    // cleanup
    ASSERT_EQ(true, DeleteShm(ePerfKey));

    return;
}   // SizeUpgrade

TEST(PerfTest, ReadLarger)
{
    PerformanceCounters * perf_ptr;

    // clear any existing shm
    DeleteShm(ePerfKey);

    // create a new larger than today segment
    ASSERT_EQ(true, CreateShm(ePerfKey, sizeof(PerformanceCounters)+64));
    perf_ptr=(PerformanceCounters *)MapShm(ePerfKey);
    ASSERT_NE(perf_ptr, (void*)NULL);
    memset(perf_ptr, 0, sizeof(PerformanceCounters)+64);
    perf_ptr->SetVersion(ePerfVersion, ePerfCountEnumSize+8);
    shmdt(perf_ptr);

    // open for read
    perf_ptr=PerformanceCounters::Init(false);
    ASSERT_NE(perf_ptr, (void*)NULL);

    // cleanup
    ASSERT_EQ(true, DeleteShm(ePerfKey));

    return;
}   // ReadLarger

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
