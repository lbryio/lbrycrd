// -------------------------------------------------------------------
//
// db_list.cc
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

#include <algorithm>
#include <syslog.h>

#include "util/db_list.h"
#include "util/mutexlock.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace leveldb {

// using singleton model from comparator.cc
static port::OnceType once = LEVELDB_ONCE_INIT;
static DBListImpl * dblist=NULL;

static void InitModule()
{
    dblist=new DBListImpl;
}   // InitModule


DBListImpl * DBList()
{
    port::InitOnce(&once, InitModule);
    return(dblist);

}   // DBList


void
DBListShutdown()
{
    // retrieve point to handle any initialization/shutdown races
    DBList();
    delete dblist;

    return;

}   // DBListShutdown



DBListImpl::DBListImpl()
    : m_UserDBCount(0), m_InternalDBCount(0)
{
}   // DBListImpl::DBListImpl


bool
DBListImpl::AddDB(
    DBImpl * Dbase,
    bool IsInternal)
{
    bool ret_flag;

    SpinLock lock(&m_Lock);

    if (IsInternal)
    {
        ret_flag=m_InternalDBs.insert(Dbase).second;
        m_InternalDBCount=m_InternalDBs.size();
    }   // if
    else
    {
        ret_flag=m_UserDBs.insert(Dbase).second;
        m_UserDBCount=m_UserDBs.size();
    }   // else

    return(ret_flag);

}   // DBListImpl::AddDB


void
DBListImpl::ReleaseDB(
    DBImpl * Dbase,
    bool IsInternal)
{
    db_set_t::iterator it;
    SpinLock lock(&m_Lock);

    if (IsInternal)
    {
        it=m_InternalDBs.find(Dbase);
        if (m_InternalDBs.end()!=it)
        {
            m_InternalDBs.erase(it);
        }   // if
        m_InternalDBCount=m_InternalDBs.size();
    }   // if
    else
    {
        it=m_UserDBs.find(Dbase);
        if (m_UserDBs.end()!=it)
        {
            m_UserDBs.erase(it);
        }   // if
        m_UserDBCount=m_UserDBs.size();
    }   // else

    return;

}   // DBListImpl::ReleaseDB


size_t
DBListImpl::GetDBCount(
    bool IsInternal)
{
    size_t ret_val;

    if (IsInternal)
        ret_val=m_InternalDBCount;
    else
        ret_val=m_UserDBCount;

    return(ret_val);

}   // DBListImpl::GetDBCount


void
DBListImpl::ScanDBs(
    bool IsInternal,
    void (DBImpl::* Function)())
{
    db_set_t::iterator it, first, last;
    SpinLock lock(&m_Lock);

    size_t count;

    // for_each() would have been fun, but setup deadlock
    //  scenarios
    // Now we have a race condition of us using the db object
    //  while someone is shutting it down ... hmm
    if (IsInternal)
    {
        first=m_InternalDBs.begin();
        last=m_InternalDBs.end();
        count=m_InternalDBs.size();
    }   // if
    else
    {
        first=m_UserDBs.begin();
        last=m_UserDBs.end();
        count=m_UserDBs.size();
    }   // else

#if 0  // for debugging ... sometimes
    m_Lock.Unlock(); /// might not be needed now
    syslog(LOG_ERR, "count %zd, total memory %" PRIu64 ", db cache size %" PRIu64 ", internal %d",
           count, gFlexCache.GetTotalMemory(), gFlexCache.GetDBCacheCapacity(IsInternal),
           (int)IsInternal);
    m_Lock.Lock();
#else
    count=count*2;  // kill off compiler warning
#endif

    // call member function of each database
    for (it=first; last!=it; ++it)
    {
        // must protect list from db add/delete during scan, leave locks
        ((*it)->*Function)();
    }   // for

    return;

}   // DBListImpl::ScanDBs

}  // namespace leveldb
