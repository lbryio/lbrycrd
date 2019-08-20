// -------------------------------------------------------------------
//
// flexcache.h
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

#include "util/cache2.h"

#ifndef STORAGE_LEVELDB_INCLUDE_FLEXCACHE_H_
#define STORAGE_LEVELDB_INCLUDE_FLEXCACHE_H_

namespace leveldb
{

// Constants declared in style of db/dbformat.h
namespace flex
{

   static const uint64_t kRlimSizeIsSmall = 2*1024*1024*1024ULL;  // above 2G is lots of ram
   static const uint64_t kRlimSmall = 256*1024*1024ULL;
   static const uint64_t kRlimLargeReserve = 1024*1024*1024ULL;
   static const uint64_t kDefaultMemory = 340*1024*1024ULL;
   static const uint64_t kMinimumDBMemory = 10*1024*1024ULL;

}   // namespace flex

/**
 * FlexCache tunes file cache versus block cache versus number
 *  of open databases
 */

class FlexCache
{
public:
    FlexCache();

    uint64_t GetDBCacheCapacity(bool IsInternalDB);

    void SetTotalMemory(uint64_t Total);

    void RecalculateAllocations() {SetTotalMemory(0);};

    uint64_t GetTotalMemory() const {return(m_TotalMemory);};

protected:

    uint64_t m_TotalMemory; //!< complete memory assigned to all FlexCache clients

};  // class FlexCache


extern FlexCache gFlexCache;

}  // namespace leveldb

#endif   // STORAGE_LEVELDB_INCLUDE_FLEXCACHE_H_
