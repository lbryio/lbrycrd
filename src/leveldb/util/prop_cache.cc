// -------------------------------------------------------------------
//
// prop_cache.cc
//
// Copyright (c) 2016-2017 Basho Technologies, Inc. All Rights Reserved.
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

#include "port/port.h"
#include "util/prop_cache.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/throttle.h"

namespace leveldb {

/**
 * lPropCacheLock and lPropCache exist to address race condition
 *  where Erlang respond to an information request after telling
 *  leveldb to shutdown.
 */
static port::Spin lPropCacheLock;
static PropertyCachePtr_t lPropCache;

/**
 * Create the cache.  Called only once upon
 * leveldb initialization
 */
void
PropertyCache::InitPropertyCache(
    EleveldbRouter_t Router)
{
    ShutdownPropertyCache();
    lPropCache = new PropertyCache(Router);

    return;

}   // PropertyCache


void
PropertyCache::ShutdownPropertyCache()
{

    SpinLock l(&lPropCacheLock);
    lPropCache.reset();

}  // PropertyCache::ShutdownPropertyCache


/**
 * Unit test support.  Allows use of derived versions
 *  of PropertyCache that easy testing
 */
void
PropertyCache::SetGlobalPropertyCache(
    PropertyCache * NewGlobal)
{
    // (creates infinite loop) ShutdownPropertyCache();
    lPropCache = NewGlobal;

    return;

}   // PropertyCache::SetGlobalPropertyCache


/**
 * Unit test support.  Allows use of derived versions
 *  of PropertyCache that easy testing
 */
Cache &
PropertyCache::GetCache()
{

    return(*lPropCache->GetCachePtr());

}   // PropertyCache::GetCache


/**
 * Unit test support.  Destroy current cache, start new ond
 */
void
PropertyCache::Flush()
{
    PropertyCachePtr_t ptr;

    // stablize the object by locking it and
    //  getting a reference count.  Flush while
    //  holding lock to keep others away
    //  ... anyone already using the object may segfault
    //      this is so dangerous ... only for testing
    {
        SpinLock l(&lPropCacheLock);
        ptr=lPropCache;

        if (NULL!=ptr.get())
            ptr->FlushInternal();
    }

}   // PropertyCache::Flush


/**
 * Construct property cache object (likely singleton)
 */
PropertyCache::PropertyCache(
    EleveldbRouter_t Router)
    : m_Cache(NULL), m_Router(Router),
      m_Cond(&m_Mutex)
{
    m_Cache = NewLRUCache2(GetCacheLimit());

}   // PopertyCache::PropertyCache


PropertyCache::~PropertyCache()
{
    delete m_Cache;
    m_Cache=NULL;
}   // PropertyCache::~PropertyCache


/**
 * used by unit & integration tests, must protect against
 *  background AAE operation requests
 */
void
PropertyCache::FlushInternal()
{
    delete m_Cache;
    m_Cache = NewLRUCache2(GetCacheLimit());

    return;

}   // PropertyCache::FlushInternal


/**
 * Retrieve property from cache if available,
 *  else call out to Riak to get properties
 */
Cache::Handle *
PropertyCache::Lookup(
    const Slice & CompositeBucket)
{
    Cache::Handle * ret_handle(NULL);
    PropertyCachePtr_t ptr;

    // race condition ... lPropCache going away as ptr assigned
    //   (unlikely here, but seen in Insert)
    {
        SpinLock l(&lPropCacheLock);
        ptr=lPropCache;
    }   // lock

    if (NULL!=ptr.get())
    {
        ret_handle=ptr->LookupInternal(CompositeBucket);
    }   // if

    return(ret_handle);

}   // PropertyCache::Lookup


/**
 * Test if global cache is running,
 *  does NOT imply it will stay valid
 */
bool
PropertyCache::Valid()
{
    PropertyCachePtr_t ptr;
    bool ret_flag(false);

    // race condition ... lPropCache going away as ptr assigned
    //   (unlikely here, but seen in Insert)
    {
        SpinLock l(&lPropCacheLock);
        ptr=lPropCache;
    }   // lock

    if (NULL!=ptr.get())
    {
        ret_flag=(NULL!=ptr->m_Cache);
    }   // if

    return(ret_flag);

}   // PropertyCache::Valid


/**
 * Retrieve property from cache if available,
 *  else call out to Riak to get properties
 */
Cache::Handle *
PropertyCache::LookupInternal(
    const Slice & CompositeBucket)
{
    Cache::Handle * ret_handle(NULL);

    if (NULL!=m_Cache)
    {
        ret_handle=m_Cache->Lookup(CompositeBucket);

        // force a reread of properties every 5 minutes
        if (NULL!=ret_handle)
        {
            uint64_t now;
            ExpiryModule * mod_ptr;

            now=GetCachedTimeMicros();
            mod_ptr=(ExpiryModule *)m_Cache->Value(ret_handle);

            // some unit tests of mod_ptr of NULL
            if (NULL!=mod_ptr && 0!=mod_ptr->ExpiryModuleExpiryMicros()
                && mod_ptr->ExpiryModuleExpiryMicros()<now)
            {
                m_Cache->Release(ret_handle);
                m_Cache->Erase(CompositeBucket);
                ret_handle=NULL;
            }   // if
        }   // if

        // not waiting in the cache already.  Request info
        if (NULL==ret_handle && NULL!=m_Router)
        {
            // call to Riak required
            ret_handle=LookupWait(CompositeBucket);
            gPerfCounters->Inc(ePerfPropCacheMiss);
        }   // if
        else if (NULL!=ret_handle)
        {
            // cached or no router
            gPerfCounters->Inc(ePerfPropCacheHit);
        }   // else if
    }   // if

    // never supposed to be missing if property cache in play
    if (NULL==ret_handle)
        gPerfCounters->Inc(ePerfPropCacheError);

    return(ret_handle);

}   // PropertyCache::LookupInternal


/**
 * Callback function used when Cache drops an object
 *  to make room for another due to cache size being exceeded
 */
static void
DeleteProperty(
    const Slice& key,
    void* value)
{
    ExpiryModuleOS * expiry;

    expiry=(ExpiryModuleOS *)value;

    delete expiry;
}   // static DeleteProperty


/**
 * (static) Add / Overwrite key in property cache.  Manage handle
 *  on caller's behalf
 */
bool
PropertyCache::Insert(
    const Slice & CompositeBucket,
    void * Props,
    Cache::Handle ** OutputPtr)
{
    PropertyCachePtr_t ptr;
    bool ret_flag(false);
    Cache::Handle * ret_handle(NULL);

    // race condition ... lPropCache going away as ptr assigned
    {
        SpinLock l(&lPropCacheLock);
        ptr=lPropCache;
    }   // lock

    if (NULL!=ptr.get() && NULL!=ptr->GetCachePtr())
    {
        ret_handle=ptr->InsertInternal(CompositeBucket, Props);

        if (NULL!=OutputPtr)
            *OutputPtr=ret_handle;
        else if (NULL!=ret_handle)
            GetCache().Release(ret_handle);

        ret_flag=(NULL!=ret_handle);
    }   // if

    return(ret_flag);

}   // PropertyCache::Insert


Cache::Handle *
PropertyCache::InsertInternal(
    const Slice & CompositeBucket,
    void * Props)
{
    assert(NULL!=m_Cache);

    Cache::Handle * ret_handle(NULL);

    {
        MutexLock lock(&m_Mutex);

        ret_handle=m_Cache->Insert(CompositeBucket, Props, 1, DeleteProperty);
        m_Cond.SignalAll();
    }

    return(ret_handle);

}   // PropertyCache::InsertInternal

}  // namespace leveldb
