// -------------------------------------------------------------------
//
// throttle.cc
//
// Copyright (c) 2011-2017 Basho Technologies, Inc. All Rights Reserved.
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

#include <sys/time.h>
#include <unistd.h>

#include "leveldb/perf_count.h"
#include "leveldb/env.h"

#include "db/db_impl.h"
#include "util/cache2.h"
#include "util/db_list.h"
#include "util/flexcache.h"
#include "util/hot_threads.h"
#include "util/thread_tasks.h"
#include "util/throttle.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace leveldb {

// mutex and condition variable objects for use in the code below
port::Mutex* gThrottleMutex=NULL;
port::CondVar* gThrottleCond=NULL;

// current time, on roughly a 60 second scale
//  (used to reduce number of OS calls for expiry)
uint64_t gCurrentTime=0;

#define THROTTLE_SECONDS 60
#define THROTTLE_TIME THROTTLE_SECONDS*1000000
#define THROTTLE_INTERVALS 63
// following is a heristic value, determined by trial and error.
//  its job is slow down the rate of change in the current throttle.
//  do not want sudden changes in one or two intervals to swing
//  the throttle value wildly.  Goal is a nice, even throttle value.
#define THROTTLE_SCALING 17

struct ThrottleData_t
{
    uint64_t m_Micros;
    uint64_t m_Keys;
    uint64_t m_Backlog;
    uint64_t m_Compactions;
};

// this array stores compaction statistics used in throttle calculation.
//  Index 0 of this array accumulates the current minute's compaction data for level 0.
//  Index 1 accumulates accumulates current minute's compaction
//  statistics for all other levels.  Remaining intervals contain
//  most recent interval statistics for last hour.
ThrottleData_t gThrottleData[THROTTLE_INTERVALS];

uint64_t gThrottleRate, gUnadjustedThrottleRate;

static volatile bool gThrottleRunning=false;
static pthread_t gThrottleThreadId;

static void * ThrottleThread(void * arg);


void
ThrottleInit()
{
    gThrottleMutex = new port::Mutex;
    gThrottleCond = new port::CondVar(gThrottleMutex);

    memset(&gThrottleData, 0, sizeof(gThrottleData));
    gThrottleRate=0;
    gUnadjustedThrottleRate=0;

    // addresses race condition during fast start/stop
    {
        MutexLock lock(gThrottleMutex);

        pthread_create(&gThrottleThreadId, NULL,  &ThrottleThread, NULL);

        while(!gThrottleRunning)
            gThrottleCond->Wait();
    }   // mutex

    return;

}   // ThrottleInit


static void *
ThrottleThread(
    void * /*arg*/)
{
    uint64_t tot_micros, tot_keys, tot_backlog, tot_compact;
    int replace_idx, loop, ret_val;
    uint64_t new_throttle, new_unadjusted;
    time_t now_seconds, cache_expire;
    struct timespec wait_time;

    replace_idx=2;
    now_seconds=0;
    cache_expire=0;
    new_unadjusted=1;

    // addresses race condition during fast start/stop
    {
        MutexLock lock(gThrottleMutex);
        gThrottleRunning=true;
        gThrottleCond->Signal();
    }   // mutex

    while(gThrottleRunning)
    {
        // update our global clock, not intended to be a precise
        //  60 second interval.
        gCurrentTime=port::TimeMicros();

        //
        // This is code polls for existance of /etc/riak/perf_counters and sets
        //  the global gPerfCountersDisabled accordingly.
        //  Sure, there should be a better place for this code.  But fits here nicely today.
        //
        ret_val=access("/etc/riak/perf_counters", F_OK);
        gPerfCountersDisabled=(-1==ret_val);

        //
        // start actual throttle work
        //
        {
            // lock gThrottleMutex while we update gThrottleData and
            // wait on gThrottleCond
            MutexLock lock(gThrottleMutex);

            // sleep 1 minute
#if _POSIX_TIMERS >= 200801L
            clock_gettime(CLOCK_REALTIME, &wait_time);
#else
            struct timeval tv;
            gettimeofday(&tv, NULL);
            wait_time.tv_sec=tv.tv_sec;
            wait_time.tv_nsec=tv.tv_usec*1000;
#endif

            now_seconds=wait_time.tv_sec;
            wait_time.tv_sec+=THROTTLE_SECONDS;
            if (gThrottleRunning) { // test in case of race at shutdown
                gThrottleCond->Wait(&wait_time);
            }
            gThrottleData[replace_idx]=gThrottleData[1];
            gThrottleData[replace_idx].m_Backlog=0;
            memset(&gThrottleData[1], 0, sizeof(gThrottleData[1]));
        } // unlock gThrottleMutex

        tot_micros=0;
        tot_keys=0;
        tot_backlog=0;
        tot_compact=0;

        // this could be faster by keeping running totals and
        //  subtracting [replace_idx] before copying [0] into it,
        //  then adding new [replace_idx].  But that needs more
        //  time for testing.
        for (loop=2; loop<THROTTLE_INTERVALS; ++loop)
        {
            tot_micros+=gThrottleData[loop].m_Micros;
            tot_keys+=gThrottleData[loop].m_Keys;
            tot_backlog+=gThrottleData[loop].m_Backlog;
            tot_compact+=gThrottleData[loop].m_Compactions;
        }   // for

        // lock gThrottleMutex while we update gThrottleData
        {
            MutexLock lock(gThrottleMutex);

            // capture current state of level-0 and other levels' backlog
            gThrottleData[replace_idx].m_Backlog=gCompactionThreads->m_WorkQueueAtomic;
            gPerfCounters->Add(ePerfThrottleBacklog1, gThrottleData[replace_idx].m_Backlog);

            gThrottleData[0].m_Backlog=gLevel0Threads->m_WorkQueueAtomic;
            gPerfCounters->Add(ePerfThrottleBacklog0, gThrottleData[0].m_Backlog);

            // non-level0 data available?
            if (0!=tot_keys)
            {
                if (0==tot_compact)
                    tot_compact=1;

                // average write time for level 1+ compactions per key
                //   times the average number of tasks waiting
                //   ( the *100 stuff is to exploit fractional data in integers )
                new_throttle=((tot_micros*100) / tot_keys)
                    * ((tot_backlog*100) / tot_compact);

                new_throttle /= 10000;  // remove *100 stuff
                //new_throttle /= gCompactionThreads->m_Threads.size();      // number of general compaction threads

                if (0==new_throttle)
                    new_throttle=1;     // throttle must have an effect

                new_unadjusted=(tot_micros*100) / tot_keys;
                new_unadjusted /= 100;
                if (0==new_unadjusted)
                    new_unadjusted=1;
            }   // if

            // attempt to most recent level0
            //  (only use most recent level0 until level1+ data becomes available,
            //   useful on restart of heavily loaded server)
            else if (0!=gThrottleData[0].m_Keys && 0!=gThrottleData[0].m_Compactions)
            {
                new_throttle=(gThrottleData[0].m_Micros / gThrottleData[0].m_Keys)
                    * (gThrottleData[0].m_Backlog / gThrottleData[0].m_Compactions);

                new_unadjusted=(gThrottleData[0].m_Micros / gThrottleData[0].m_Keys);
                if (0==new_unadjusted)
                    new_unadjusted=1;
            }   // else if
            else
            {
                new_throttle=1;
            }   // else

            // change the throttle slowly
            //  (+1 & +2 keep throttle moving toward goal when difference new and
            //   old is less than THROTTLE_SCALING)
            int temp_rate;

            temp_rate=gThrottleRate;
            if (temp_rate < new_throttle)
                temp_rate+=(new_throttle - temp_rate)/THROTTLE_SCALING +1;
            else
                temp_rate-=(temp_rate - new_throttle)/THROTTLE_SCALING +2;

            // +2 can make this go negative
            if (temp_rate<1)
                temp_rate=1;   // throttle must always have an effect

            gThrottleRate=temp_rate;
            gUnadjustedThrottleRate=new_unadjusted;

	    // Log(NULL, "ThrottleRate %" PRIu64 ", UnadjustedThrottleRate %" PRIu64, gThrottleRate, gUnadjustedThrottleRate);

            gPerfCounters->Set(ePerfThrottleGauge, gThrottleRate);
            gPerfCounters->Add(ePerfThrottleCounter, gThrottleRate*THROTTLE_SECONDS);
            gPerfCounters->Set(ePerfThrottleUnadjusted, gUnadjustedThrottleRate);

            // prepare for next interval
            memset(&gThrottleData[0], 0, sizeof(gThrottleData[0]));
        } // unlock gThrottleMutex

        ++replace_idx;
        if (THROTTLE_INTERVALS==replace_idx)
            replace_idx=2;

        //
        // This is code to manage / flush the flexcache's old file cache entries.
        //  Sure, there should be a better place for this code.  But fits here nicely today.
        //
        if (cache_expire < now_seconds)
        {
            cache_expire = now_seconds + 60*60;  // hard coded to one hour for now
            DBList()->ScanDBs(true,  &DBImpl::PurgeExpiredFileCache);
            DBList()->ScanDBs(false, &DBImpl::PurgeExpiredFileCache);
        }   // if

        //
        // This is a second non-throttle task added to this one minute loop.  Pattern forming.
        //  See if hot backup wants to initiate.
        //
	CheckHotBackupTrigger();

        // nudge compaction logic of potential grooming
        if (0==gCompactionThreads->m_WorkQueueAtomic)  // user databases
            DBList()->ScanDBs(false, &DBImpl::CheckAvailableCompactions);
        if (0==gCompactionThreads->m_WorkQueueAtomic)  // internal databases
            DBList()->ScanDBs(true,  &DBImpl::CheckAvailableCompactions);

    }   // while

    return(NULL);

}   // ThrottleThread


void SetThrottleWriteRate(uint64_t Micros, uint64_t Keys, bool IsLevel0)
{
    if (IsLevel0)
    {
        // lock gThrottleMutex while we update gThrottleData
        {
            MutexLock lock(gThrottleMutex);

            gThrottleData[0].m_Micros+=Micros;
            gThrottleData[0].m_Keys+=Keys;
            gThrottleData[0].m_Backlog=0;
            gThrottleData[0].m_Compactions+=1;
        } // unlock gThrottleMutex

        gPerfCounters->Add(ePerfThrottleMicros0, Micros);
        gPerfCounters->Add(ePerfThrottleKeys0, Keys);
        gPerfCounters->Inc(ePerfThrottleCompacts0);
    }   // if

    else
    {
        // lock gThrottleMutex while we update gThrottleData
        {
            MutexLock lock(gThrottleMutex);

            gThrottleData[1].m_Micros+=Micros;
            gThrottleData[1].m_Keys+=Keys;
            gThrottleData[1].m_Backlog=0;
            gThrottleData[1].m_Compactions+=1;
        } // unlock gThrottleMutex

        gPerfCounters->Add(ePerfThrottleMicros1, Micros);
        gPerfCounters->Add(ePerfThrottleKeys1, Keys);
        gPerfCounters->Inc(ePerfThrottleCompacts1);
    }   // else

    return;
};

uint64_t GetThrottleWriteRate() {return(gThrottleRate);};
uint64_t GetUnadjustedThrottleWriteRate() {return(gUnadjustedThrottleRate);};

// clock_gettime but only updated once every 60 seconds (roughly)
uint64_t GetCachedTimeMicros() {return(gCurrentTime);};
void SetCachedTimeMicros(uint64_t Time) {gCurrentTime=Time;};
/**
 * ThrottleStopThreads() is the first step in a two step shutdown.
 * This stops the 1 minute throttle calculation loop that also
 * can initiate leveldb compaction actions.  Background compaction
 * threads should stop between these two steps.
 */
void ThrottleStopThreads()
{
    if (gThrottleRunning)
    {
        gThrottleRunning=false;

        // lock gThrottleMutex so that we can signal gThrottleCond
        {
            MutexLock lock(gThrottleMutex);
            gThrottleCond->Signal();
        } // unlock gThrottleMutex

        pthread_join(gThrottleThreadId, NULL);
    }   // if

    return;

}   // ThrottleShutdown

/**
 * ThrottleClose is the second step in a two step shutdown of
 *  throttle.  The intent is for background compaction threads
 *  to stop between these two steps.
 */
void ThrottleClose()
{
    // safety check
    if (gThrottleRunning)
        ThrottleStopThreads();

    delete gThrottleCond;
    gThrottleCond = NULL;

    delete gThrottleMutex;
    gThrottleMutex = NULL;

    return;
}   // ThrottleShutdown

}  // namespace leveldb
