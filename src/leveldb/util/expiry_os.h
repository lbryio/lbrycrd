// -------------------------------------------------------------------
//
// expiry_os.h
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

#ifndef EXPIRY_OS_H
#define EXPIRY_OS_H

#include <vector>

#include "leveldb/options.h"
#include "leveldb/expiry.h"
#include "leveldb/perf_count.h"
#include "db/dbformat.h"
#include "db/version_edit.h"

namespace leveldb
{

class ExpiryModuleOS : public ExpiryModule
{
public:
    ExpiryModuleOS()
        : expiry_enabled(false), expiry_minutes(0),
        expiry_unlimited(false), whole_file_expiry(false)
    {};

    virtual ~ExpiryModuleOS() {};

    // Print expiry options to LOG file
    virtual void Dump(Logger * log) const;

    // Quick test to allow manifest logic and such know if
    //  extra expiry logic should be checked
    virtual bool ExpiryActivated() const {return(expiry_enabled);};

    // db/write_batch.cc MemTableInserter::Put() calls this.
    // returns false on internal error
    virtual bool MemTableInserterCallback(
        const Slice & Key,   // input: user's key about to be written
        const Slice & Value, // input: user's value object
        ValueType & ValType,   // input/output: key type. call might change
        ExpiryTimeMicros & Expiry) const;  // input/output: 0 or specific expiry. call might change

    // db/dbformat.cc KeyRetirement::operator() calls this.
    // db/version_set.cc SaveValue() calls this too.
    // returns true if key is expired, returns false if key not expired
    virtual bool KeyRetirementCallback(
        const ParsedInternalKey & Ikey) const;  // input: key to examine for retirement

    // table/table_builder.cc TableBuilder::Add() calls this.
    // returns false on internal error
    virtual bool TableBuilderCallback(
        const Slice & key,       // input: internal key
        SstCounters & counters) const; // input/output: counters for new sst table

    // db/memtable.cc MemTable::Get() calls this.
    // returns true if type/expiry is expired, returns false if not expired
    virtual bool MemTableCallback(
        const Slice & Key) const;    // input: leveldb internal key

    // db/version_set.cc VersionSet::Finalize() calls this if no
    //  other compaction selected for a level
    // returns true if there is an expiry compaction eligible
    virtual bool CompactionFinalizeCallback(
        bool WantAll,                  // input: true - examine all expired files
        const Version & Ver,           // input: database state for examination
        int Level,                     // input: level to review for expiry
        VersionEdit * Edit) const;     // output: NULL or destination of delete list

    // utility to CompactionFinalizeCallback to review
    //  characteristics of one SstFile to see if entirely expired
    virtual bool IsFileExpired(const FileMetaData & SstFile, ExpiryTimeMicros Now) const;

    // Accessors to option parameters
    bool IsExpiryEnabled() const {return(expiry_enabled);};
    void SetExpiryEnabled(bool Flag=true) {expiry_enabled=Flag;};

    bool IsExpiryUnlimited() const {return(expiry_unlimited);};
    void SetExpiryUnlimited(bool Flag=true) {expiry_unlimited=Flag;};

    uint64_t GetExpiryMinutes() const {return(expiry_minutes);};
    void SetExpiryMinutes(uint64_t Minutes) {expiry_minutes=Minutes; expiry_unlimited=false;};

    bool IsWholeFileExpiryEnabled() const {return(whole_file_expiry);};
    void SetWholeFileExpiryEnabled(bool Flag=true) {whole_file_expiry=Flag;};

public:
    // NOTE: option names below are intentionally public and lowercase with underscores.
    //       This is to match style of options within include/leveldb/options.h.

    // Riak specific option to enable/disable expiry features globally
    //  true: expiry enabled
    //  false: disabled (some expired keys may reappear)
    bool expiry_enabled;

    // Riak specific option giving number of minutes a stored key/value
    // may stay within the database before automatic deletion.  Zero
    // disables expiry by age feature.
    uint64_t expiry_minutes;
    bool expiry_unlimited;

    // Riak specific option authorizing leveldb to eliminate entire
    // files that contain expired data (delete files instead of
    // removing expired data during compactions).
    bool whole_file_expiry;

protected:
    // When "creating" write time, chose its source based upon
    //  open source versus enterprise edition
    virtual uint64_t GenerateWriteTimeMicros(const Slice & Key, const Slice & Value) const;


};  // ExpiryModuleOS

uint64_t CuttlefishDurationMinutes(const char * Buf);

}  // namespace leveldb

#endif // ifndef
