// -------------------------------------------------------------------
//
// db_list.h
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

#include "db/db_impl.h"
#include "port/port.h"


namespace leveldb
{

/**
 * DBList:  class to provide management access to all
 *  open databases (Riak vnodes)
 */
class DBListImpl
{
protected:
   typedef std::set<DBImpl *> db_set_t;

   port::Spin m_Lock;      //!< thread protection for set
   db_set_t m_UserDBs;     //!< set of pointers for user db
   db_set_t m_InternalDBs; //!< Riak internal dbs

   volatile size_t m_UserDBCount;   //!< m_UserDBs size() for non-blocking retrieval
   volatile size_t m_InternalDBCount;   //!< m_InternalDBs size() for non-blocking retrieval

public:
   DBListImpl();
   virtual ~DBListImpl() {};

   bool AddDB(DBImpl *, bool is_internal);
   void ReleaseDB(DBImpl *, bool is_internal);

   size_t GetDBCount(bool is_internal);

   void ScanDBs(bool is_internal, void (DBImpl::*)());

};  // class DBListImpl


// Universal access to dblist ... initialization order independent
DBListImpl * DBList();

// cleanup memory, mostly for valgrind
void DBListShutdown();


}  // namespace leveldb
