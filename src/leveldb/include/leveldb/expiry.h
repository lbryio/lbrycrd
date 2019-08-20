// -------------------------------------------------------------------
//
// expiry.h:  background expiry management for Basho's modified leveldb
//
// Copyright (c) 2016 Basho Technologies, Inc. All Rights Reserved.
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

#ifndef EXPIRY_H
#define EXPIRY_H

#include <limits.h>
#include <stdint.h>
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "util/refobject_base.h"

namespace leveldb {

class Compaction;
class Logger;
struct ParsedInternalKey;
class Slice;
class SstCounters;
class Version;
class VersionEdit;
struct FileMetaData;


enum EleveldbRouterActions_t
{
    eGetBucketProperties=1
};  // enum EleveldbRouterActions_t


typedef bool (* EleveldbRouter_t)(EleveldbRouterActions_t Action, int ParamCount, const void ** Params);


class ExpiryModule : public RefObjectBase
{
public:
    virtual ~ExpiryModule() {};

    // Print expiry options to LOG file
    virtual void Dump(Logger * log) const
    {Log(log,"                        Expiry: (none)");};

    // Quick test to allow manifest logic and such know if
    //  extra expiry logic should be checked
    virtual bool ExpiryActivated() const {return(false);};

    // db/write_batch.cc MemTableInserter::Put() calls this.
    // returns false on internal error
    virtual bool MemTableInserterCallback(
        const Slice & Key,   // input: user's key about to be written
        const Slice & Value, // input: user's value object
        ValueType & ValType,   // input/output: key type. call might change
        ExpiryTimeMicros & Expiry) const  // input/output: 0 or specific expiry. call might change
    {return(true);};

    // db/dbformat.cc KeyRetirement::operator() calls this.
    // db/version_set.cc SaveValue() calls this too.
    // returns true if key is expired, returns false if key not expired
    virtual bool KeyRetirementCallback(
        const ParsedInternalKey & Ikey) const
    {return(false);};

    // table/table_builder.cc TableBuilder::Add() calls this.
    // returns false on internal error
    virtual bool TableBuilderCallback(
        const Slice & Key,       // input: internal key
        SstCounters & Counters) const // input/output: counters for new sst table
    {return(true);};

    // db/memtable.cc MemTable::Get() calls this.
    // returns true if type/expiry is expired, returns false if not expired
    virtual bool MemTableCallback(
        const Slice & Key) const        // input: leveldb internal key
    {return(false);};

    // db/version_set.cc VersionSet::Finalize() calls this if no
    //  other compaction selected for a level
    // returns true if there is an expiry compaction eligible
    virtual bool CompactionFinalizeCallback(
        bool WantAll,                 // input: true - examine all expired files
        const Version & Ver,          // input: database state for examination
        int Level,                    // input: level to review for expiry
        VersionEdit * Edit) const     // output: NULL or destination of delete list
    {return(false);};

    // yep, sometimes we want to expiry this expiry module object.
    //  mostly for bucket level properties in Riak EE
    virtual uint64_t ExpiryModuleExpiryMicros() {return(0);};

    // Creates derived ExpiryModule object that matches compile time
    //  switch for open source or Basho enterprise edition features.
    static ExpiryModule * CreateExpiryModule(EleveldbRouter_t Router);

    // Cleans up global objects related to expiry
    //  switch for open source or Basho enterprise edition features.
    static void ShutdownExpiryModule();

    // Riak EE:  stash a user created module with settings
    virtual void NoteUserExpirySettings() {};

protected:
    ExpiryModule() {};

private:
    ExpiryModule(const ExpiryModule &);
    ExpiryModule & operator=(const ExpiryModule &);

};  // ExpiryModule


typedef RefPtr<class ExpiryModule> ExpiryPtr_t;

} // namespace leveldb

#endif // ifndef

