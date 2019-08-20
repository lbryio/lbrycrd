// -------------------------------------------------------------------
//
// flexcache.cc
//
// Copyright (c) 2011-2013 Basho Technologies, Inc. All Rights Reserved.
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
#include <sys/resource.h>

#include "util/db_list.h"
#include "util/flexcache.h"

namespace leveldb {


// global cache control
FlexCache gFlexCache;


/**
 * Initialize object
 */
FlexCache::FlexCache()
    : m_TotalMemory(0)
{
    struct rlimit limit;
    int ret_val;

    // initialize total memory available based upon system data
    ret_val=getrlimit(RLIMIT_DATA, &limit);

    //  unsigned long caste to fix warning in smartos1.8, smartos 13.1, solaris10
    if (0==ret_val && (unsigned long)RLIM_INFINITY!=limit.rlim_max)
    {
        // 2Gig is "small ram", Riak going to be tight
       if (limit.rlim_max < flex::kRlimSizeIsSmall)
            m_TotalMemory=flex::kRlimSmall;
        else
            m_TotalMemory=(limit.rlim_max - flex::kRlimLargeReserve) / 2;
    }   // if

    // create a default similar to Google's original,
    //  but enough for 2 vnodes including Riak default buffer sizes
    else
    {
        m_TotalMemory=flex::kDefaultMemory;
    }   // else

    return;

}   // FlexCache::FlexCache


/**
 * Return current capacity limit for cache flavor indicated,
 *  default is zero if unknown flavor.
 */
uint64_t
FlexCache::GetDBCacheCapacity(
    bool IsInternal)   //!< value describing cache attributes of caller
{
    uint64_t ret_val, shared_total;
    size_t count, internal_count;

    // get count of database by type
    count=DBList()->GetDBCount(IsInternal);
    if (IsInternal)
        internal_count=count;
    else
        internal_count=DBList()->GetDBCount(true);

    // what is total memory assigned to a type
    if (IsInternal)
        shared_total=(m_TotalMemory*2)/10;  // integer *.2
    else if (0!=internal_count)
        shared_total=(m_TotalMemory*8)/10;
    else // no internal database
        shared_total=m_TotalMemory;

    // split up type specific aggregate to "per database" value
    if (0!=count)
        ret_val=shared_total / count;
    else
        ret_val=shared_total;

    return(ret_val);

}   // FlexCache::GetDBCacheCapacity


/**
 * Change the memory allocated to all caches, and actively resize
 *  existing caches
 */
void
FlexCache::SetTotalMemory(
    uint64_t Total)    //!< new memory allocated to all caches
{
    // only review current allocation if new value is different
    //  and not zero default
    if (0!=Total && Total!=m_TotalMemory)
    {
        m_TotalMemory=Total;
    }   // if

    DBList()->ScanDBs(true, &DBImpl::ResizeCaches);
    DBList()->ScanDBs(false, &DBImpl::ResizeCaches);

    return;

}   // FlexCache::SetTotalMemory

}  // namespace leveldb
