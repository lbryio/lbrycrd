// -------------------------------------------------------------------
//
// prop_cache.h
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

#ifndef PROP_CACHE_H
#define PROP_CACHE_H

#include "leveldb/cache.h"
#include "util/expiry_os.h"
#include "util/refobject_base.h"
#include "port/port.h"


namespace leveldb
{

class PropertyCache : public RefObjectBase
{
public:
    /**
     * static functions are API for production usage
     */

    // create global cache object
    static void InitPropertyCache(EleveldbRouter_t Router);

    // release global cache object
    static void ShutdownPropertyCache();

    // unit test support
    static void SetGlobalPropertyCache(PropertyCache * NewCache);

    // static lookup, usually from CachePtr
    static Cache::Handle * Lookup(const Slice & CompositeBucket);

    // static insert, usually from eleveldb::property_cache()
    static bool Insert(const Slice & CompositeBucket, void * Props, Cache::Handle ** OutputPtr);

    // static retrieval of active cache
    static Cache & GetCache();

    // for unit tests, "flush" cache
    static void Flush();

    // test if cache is running (so OS builds know to ignore)
    static bool Valid();

    // virtual destructor to facilitate unit tests
    virtual ~PropertyCache();

protected:
    /**
     * protected functions are API for unit tests.  The static functions
     *  route program flow to these.
     */

    // only allow creation from InitPropertyCache() or unit tests
    PropertyCache(EleveldbRouter_t);

    // accessor to m_Cache pointer (really bad if NULL m_Cache)
    Cache * GetCachePtr() {return(m_Cache);};

    // unit & integration test support to get rid of current cache entries
    void FlushInternal();

    // internal equivalent to static Lookup() function
    Cache::Handle * LookupInternal(const Slice & CompositeBucket);

    // internal routine to launch lookup request via eleveldb router, then wait
    Cache::Handle * LookupWait(const Slice & CompositeBucket);

    // internal routine to insert object and signal condition variable
    Cache::Handle * InsertInternal(const Slice & CompositeBucket, void * Props);

    // 1000 is number of cache entries.  Just pulled
    //  that number out of the air.
    // virtual for unit test to override
    virtual int GetCacheLimit() const {return(1000);}

    Cache * m_Cache;
    EleveldbRouter_t m_Router;
    port::Mutex m_Mutex;
    port::CondVar m_Cond;

// The follow explicitly disable use of default constructor, copy constructor,
//  and assignment operator.
private:
    PropertyCache();
    PropertyCache(const PropertyCache &);
    PropertyCache operator=(const PropertyCache &);

}; // class PropertyCache


/**
 * This temple wraps the entire property cache
 */
typedef RefPtr<PropertyCache> PropertyCachePtr_t;


/**
 * This template wraps an object in property cache
 *  to insure it is properly released.
 *  Makes calls to static functions of PropertyCache.
 */
template<typename Object> class CachePtr
{
    /****************************************************************
    *  Member objects
    ****************************************************************/
public:

protected:
    Cache::Handle * m_Ptr;            // NULL or object in cache

private:

    /****************************************************************
    *  Member functions
    ****************************************************************/
public:
    CachePtr() : m_Ptr(NULL) {};

    ~CachePtr() {Release();};

    // unprotected if GetCache is NULL
    void Release()
    {
        if (NULL!=m_Ptr)
            PropertyCache::GetCache().Release(m_Ptr);
        m_Ptr=NULL;
    };

    CachePtr & operator=(Cache::Handle * Hand) {reset(Hand);};

    void reset(Cache::Handle * Hand=NULL)
    {
        if (m_Ptr!=Hand)
        {
            Release();
            m_Ptr=Hand;
        }   // if
    }


    Object * get()
    {return(PropertyCache::Valid()
            ? (Object *)PropertyCache::GetCache().Value(m_Ptr)
            : NULL);};

    // unprotected if GetCache is NULL
    const Object * get() const
    {return(PropertyCache::Valid()
            ? (const Object *)PropertyCache::GetCache().Value(m_Ptr)
            : NULL);};

    Object * operator->() {return(get());};
    const Object * operator->() const {return(get());};

    Object & operator*() {return(*get());};
    const Object & operator*() const {return(*get());};

    bool Lookup(const Slice & Key)
    {
        Release();
        m_Ptr=PropertyCache::Lookup(Key);
        return(NULL!=m_Ptr);
    };

    bool Insert(const Slice & Key, Object * Value)
    {
        bool ret_flag(false);
        Release();
        ret_flag=PropertyCache::Insert(Key, (void *)Value, &m_Ptr);
        return(ret_flag);
    };

    // unprotected if GetCache is NULL
    void Erase(const Slice & Key)
    {
        Release();
        if (PropertyCache::Valid())
            PropertyCache::GetCache().Erase(Key);
        return;
    };

protected:

private:
    CachePtr(const CachePtr &);
    CachePtr & operator=(const CachePtr &);

};  // template CachePtr


typedef CachePtr<ExpiryModuleOS> ExpiryPropPtr_t;


}  // namespace leveldb

#endif // ifndef
