// -------------------------------------------------------------------
//
// refobject_base.h
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

// -------------------------------------------------------------------
//  Base class for reference-counted types; refactored from
//  eleveldb/c_src/refobjects.h and leveldb/util/thread_tasks.h
// -------------------------------------------------------------------

#ifndef LEVELDB_INCLUDE_REFOBJECT_BASE_H_
#define LEVELDB_INCLUDE_REFOBJECT_BASE_H_

#include <stdint.h>

#include "port/port.h"
#include "leveldb/atomics.h"
#include "util/mutexlock.h"

namespace leveldb {

/**
 * Base class for reference-counted types
 *
 * A user of a reference-counted object makes the reference explicit by
 * calling the RefInc() method, which increments the internal reference
 * counter in a thread safe manner. When the user of the object is done
 * with the object, it releases the reference by calling the RefDec()
 * method, which decrements the internal counter in a thread safe manner.
 * When the reference counter reaches 0, the RefDec() method deletes
 * the current object by executing a "delete this" statement.
 *
 * Note that the because RefDec() executes "delete this" when the reference
 * count reaches 0, the reference-counted object must be allocated on the
 * heap.
 */
class RefObjectBase
{
    // force this private so everyone is using memory fenced GetRefCount
 private:
    volatile uint32_t m_RefCount;

 public:
    RefObjectBase() : m_RefCount(0) {}
    virtual ~RefObjectBase() {}

    virtual uint32_t RefInc() {return(inc_and_fetch(&m_RefCount));}

    virtual uint32_t RefDec()
    {
        uint32_t current_refs;

        current_refs=dec_and_fetch(&m_RefCount);
        if (0==current_refs) {
            delete this;
        }

        return(current_refs);
    }   // RefDec

    // some derived objects might need other cleanup before delete (see ErlRefObject)
    virtual uint32_t RefDecNoDelete() {return(dec_and_fetch(&m_RefCount));};

    // establish memory fence via atomic operation call
    virtual uint32_t GetRefCount() {return(add_and_fetch(&m_RefCount, (uint32_t)0));};

 private:
    // hide the copy ctor and assignment operator (not implemented)
    RefObjectBase(const RefObjectBase&);
    RefObjectBase& operator=(const RefObjectBase&);
};


template<typename Object> class RefPtr
{
    /****************************************************************
    *  Member objects
    ****************************************************************/
public:

protected:
    port::Spin m_Spin;
    Object * m_Ptr;            // NULL or object being reference counted

private:

    /****************************************************************
    *  Member functions
    ****************************************************************/
public:
    RefPtr() : m_Ptr(NULL) {};

    virtual ~RefPtr() {RefDecrement();};

    RefPtr(const RefPtr & rhs) : m_Ptr(NULL) {reset(rhs.m_Ptr);};
    RefPtr(Object * Ptr) : m_Ptr(NULL) {reset(Ptr);};
    RefPtr(Object & Obj) : m_Ptr(NULL) {reset(&Obj);};

//    RefPtr & operator=(const Object & rhs) {reset(rhs.m_Ptr); return(*this);};
    RefPtr & operator=(Object & rhs) {reset(&rhs); return(*this);};
    RefPtr & operator=(Object * Ptr) {reset(Ptr); return(*this);};
    RefPtr & operator=(RefPtr & RPtr) {reset(RPtr.m_Ptr); return(*this);};
    RefPtr & operator=(const RefPtr & RPtr) {reset(RPtr.m_Ptr); return(*this);};

    bool operator==(const Object & Obj) const {return(m_Ptr==&Obj);};
    bool operator!=(const Object & Obj) const {return(m_Ptr!=&Obj);};
    operator void*() {return(m_Ptr);};

    // stl like functions
    void assign(Object * Ptr) {reset(Ptr);};

    void reset(Object * ObjectPtr=NULL)
    {
        SpinLock l(&m_Spin);
        Object * old_ptr;

        // increment new before decrement old in case
        //  there are any side effects / contained / circular objects
        old_ptr=m_Ptr;
        m_Ptr=ObjectPtr;

        if (NULL!=m_Ptr)
        {
            RefIncrement();
        }   // if
        // swap back for the moment
        if (NULL!=old_ptr)
        {
            m_Ptr=old_ptr;
            RefDecrement();
        }   // if

        // final pointer
        m_Ptr=ObjectPtr;
    }

    Object * get() {return(m_Ptr);};

    const Object * get() const {return(m_Ptr);};

    Object * operator->() {return(m_Ptr);};
    const Object * operator->() const {return(m_Ptr);};

    Object & operator*() {return(*get());};
    const Object & operator*() const {return(*get());};

    bool operator<(const RefPtr & rhs) const
    {return(*get()<*rhs.get());};

protected:
    // reduce reference count, delete if 0
    void RefDecrement()
    {
        if (NULL!=m_Ptr)
        {
            m_Ptr->RefDec();
            m_Ptr=NULL;
        }   // if
    };

    void RefIncrement()
    {
        if (NULL!=m_Ptr)
            m_Ptr->RefInc();
    };

private:


};  // template RefPtr


} // namespace leveldb

#endif  // LEVELDB_INCLUDE_REFOBJECT_BASE_H_
