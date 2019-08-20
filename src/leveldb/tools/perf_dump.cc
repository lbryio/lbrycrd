#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "leveldb/env.h"
#include "leveldb/perf_count.h"
#include "port/port.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

void command_help();

int
main(
    int argc,
    char ** argv)
{
    bool error_seen, csv_header, diff_mode, running;
    int error_counter;
    unsigned diff_seconds;
    char ** cursor;

    running=true;
    error_seen=false;
    error_counter=0;

    csv_header=false;
    diff_mode=false;
    diff_seconds=1;


    for (cursor=argv+1; NULL!=*cursor && running; ++cursor)
    {
        // option flag?
        if ('-'==**cursor)
        {
            char flag;

            flag=*((*cursor)+1);
            switch(flag)
            {
                case 'h':  csv_header=true; break;
                case 'd':
                    diff_mode=true;
                    ++cursor;
                    diff_seconds=strtoul(*cursor, NULL, 10);
                    break;

                default:
                    fprintf(stderr, " option \'%c\' is not valid\n", flag);
                    command_help();
                    running=false;
                    error_counter=1;
                    error_seen=true;
                    break;
            }   // switch
        }   // if

        // non flag params
        else
        {
            fprintf(stderr, " option \'%s\' is not valid\n", *cursor);
            command_help();
            running=false;
            error_counter=1;
            error_seen=true;
        }   // else
    }   // for

    // attach to shared memory if params looking good
    if (!error_seen)
    {
        const leveldb::PerformanceCounters * perf_ptr;
        bool first_pass;

        first_pass=true;
        perf_ptr=leveldb::PerformanceCounters::Init(true);

        if (NULL!=perf_ptr)
        {
            uint64_t first_time;
            int loop;

            first_time=leveldb::port::TimeMicros();

            if (csv_header)
            {
                csv_header=false;
                printf("time, diff time, name, count\n");
            }   // if

            if (diff_mode)
            {
                uint64_t prev_counters[leveldb::ePerfCountEnumSize], cur_counters[leveldb::ePerfCountEnumSize];
                uint64_t cur_time;

                do
                {
                    // capture state before reporting
                    cur_time=leveldb::port::TimeMicros();
                    for (loop=0; loop<leveldb::ePerfCountEnumSize; ++loop)
                    {
                        cur_counters[loop]=perf_ptr->Value(loop);
                    }   // for

                    if (!first_pass)
                    {
                        for (loop=0; loop<leveldb::ePerfCountEnumSize; ++loop)
                        {
                            printf("%" PRIu64 ", %" PRIu64 ", %s, %" PRIu64 "\n",
                                   cur_time, cur_time-first_time,
                                   leveldb::PerformanceCounters::GetNamePtr(loop),
                                   cur_counters[loop]-prev_counters[loop]);
                        }   // for
                    }   // if

                    first_pass=false;

                    // save for next pass
                    //  (counters are "live" so use data previously reported to maintain some consistency)
                    for (loop=0; loop<leveldb::ePerfCountEnumSize; ++loop)
                    {
                        prev_counters[loop]=cur_counters[loop];
                    }   // for

                    sleep(diff_seconds);
                } while(true);
            }   // if

            // one time dump
            else
            {
                for (loop=0; loop<leveldb::ePerfCountEnumSize; ++loop)
                {
                    printf("%" PRIu64 ", %u, %s, %" PRIu64 "\n",
                           first_time, 0,
                           leveldb::PerformanceCounters::GetNamePtr(loop),
                           perf_ptr->Value(loop));
                }   // for
            }   // else
        }   // if
        else
        {
            fprintf(stderr, "unable to attach to shared memory, error %d\n",
                    leveldb::PerformanceCounters::m_LastError);
            ++error_counter;
            error_seen=true;
        }   // else
    }   // if

    if (error_seen)
        command_help();

    return( error_seen && 0!=error_counter ? 1 : 0 );

}   // main


void
command_help()
{
    fprintf(stderr, "perf_dump [option]*\n");
    fprintf(stderr, "  options\n");
    fprintf(stderr, "      -h    print csv formatted header line (once)\n");
    fprintf(stderr, "      -d n  print diff ever n seconds\n");
}   // command_help

namespace leveldb {


}  // namespace leveldb

