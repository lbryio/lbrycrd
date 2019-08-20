// -------------------------------------------------------------------
//
// expiry_os.cc
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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <limits.h>

#include "leveldb/perf_count.h"
#include "leveldb/env.h"
#include "db/dbformat.h"
#include "db/db_impl.h"
#include "db/version_set.h"
#include "util/expiry_os.h"
#include "util/logging.h"
#include "util/throttle.h"

namespace leveldb {

// sext key for Riak's meta data
static const char * lRiakMetaDataKey=
    {"\x10\x00\x00\x00\x02\x0c\xb6\xd9\x00\x08"};
static const size_t lRiakMetaDataKeyLen=10;

/**
 * settings information that gets dumped to LOG upon
 *  leveldb start
 */
void
ExpiryModuleOS::Dump(
    Logger * log) const
{
    Log(log,"  ExpiryModuleOS.expiry_enabled: %s", IsExpiryEnabled() ? "true" : "false");
    Log(log,"  ExpiryModuleOS.expiry_minutes: %" PRIu64, GetExpiryMinutes());
    Log(log,"ExpiryModuleOS.expiry_unlimited: %s", IsExpiryUnlimited() ? "true" : "false");
    Log(log,"     ExpiryModuleOS.whole_files: %s", IsWholeFileExpiryEnabled() ? "true" : "false");

    return;

}   // ExpiryModuleOS::Dump


/**
 * db/write_batch.cc MemTableInserter() uses this to initialize
 *   expiry info.
 */
bool
ExpiryModuleOS::MemTableInserterCallback(
    const Slice & Key,   // input: user's key about to be written
    const Slice & Value, // input: user's value object
    ValueType & ValType,   // input/output: key type. call might change
    ExpiryTimeMicros & Expiry)   // input/output: 0 or specific expiry. call might change
    const
{
    bool good(true);

    // only update the expiry time if explicit type
    //  without expiry, OR ExpiryMinutes set and not internal key
    if ((kTypeValueWriteTime==ValType && 0==Expiry)
        || (kTypeValue==ValType
            && (0!=GetExpiryMinutes() || IsExpiryUnlimited())
            && IsExpiryEnabled()
            && (Key.size()<lRiakMetaDataKeyLen
                || 0!=memcmp(lRiakMetaDataKey,Key.data(),lRiakMetaDataKeyLen))))
    {
        ValType=kTypeValueWriteTime;
        Expiry=GenerateWriteTimeMicros(Key, Value);
    }   // if

    return(good);

}   // ExpiryModuleOS::MemTableInserterCallback


/**
 * Use Basho's GetCachedTimeMicros() as write time source.
 *  This clock returns microseconds since epoch, but
 *  only updates every 60 seconds or so.
 */
uint64_t
ExpiryModuleOS::GenerateWriteTimeMicros(
    const Slice & Key,
    const Slice & Value) const
{

    return(GetCachedTimeMicros());

}  // ExpiryModuleOS::GenerateWriteTimeMicros()

/**
 * Returns true if key is expired.  False if key is NOT expired
 *  (used by MemtableCallback() too).
 *  Used within dbformat.cc, db_iter.cc, & version_set.cc
 */
bool ExpiryModuleOS::KeyRetirementCallback(
    const ParsedInternalKey & Ikey) const
{
    bool is_expired(false);
    uint64_t now_micros, expires_micros;

    if (IsExpiryEnabled())
    {
        switch(Ikey.type)
        {
            case kTypeDeletion:
            case kTypeValue:
            default:
                is_expired=false;
                break;

            case kTypeValueWriteTime:
                if (0!=GetExpiryMinutes() && 0!=Ikey.expiry &&
                    !IsExpiryUnlimited())
                {
                    now_micros=GetCachedTimeMicros();
                    expires_micros=GetExpiryMinutes()*60*port::UINT64_ONE_SECOND_MICROS
                        + Ikey.expiry;
                    is_expired=(expires_micros<=now_micros);
                }   // if
                break;

            case kTypeValueExplicitExpiry:
                if (0!=Ikey.expiry)
                {
                    now_micros=GetCachedTimeMicros();
                    is_expired=(Ikey.expiry<=now_micros);
                }   // if
                break;
        }   // switch
    }   // if

    return(is_expired);

}   // ExpiryModuleOS::KeyRetirementCallback


/**
 *  - Sets low/high date range for aged expiry.
 *     (low for possible time series optimization)
 *  - Sets high date range for explicit expiry.
 *  - Increments delete counter for things already
 *     expired (to aid in starting compaction for
 *     keys tombstoning for higher levels).
 *  (called from table/table_builder.cc)
 */
bool             // return value ignored
ExpiryModuleOS::TableBuilderCallback(
    const Slice & Key,
    SstCounters & Counters) const
{
    bool good(true);
    ExpiryTimeMicros expires_micros, temp;

    if (IsExpiryKey(Key))
        expires_micros=ExtractExpiry(Key);
    else
        expires_micros=0;

    // make really high so that everything is less than it
    if (1==Counters.Value(eSstCountKeys))
        Counters.Set(eSstCountExpiry1, ULLONG_MAX);

    // only updating counters.  do this even if
    //  expiry disabled
    switch(ExtractValueType(Key))
    {
        // exp_write_low set to smallest (earliest) write time
        // exp_write_high set to largest (most recent) write time
        case kTypeValueWriteTime:
            temp=Counters.Value(eSstCountExpiry1);
            if (expires_micros<temp)
                Counters.Set(eSstCountExpiry1, expires_micros);
            if (Counters.Value(eSstCountExpiry2)<expires_micros)
                Counters.Set(eSstCountExpiry2, expires_micros);
            // add to delete count if expired already
            //   i.e. acting as a tombstone
            if (MemTableCallback(Key))
                Counters.Inc(eSstCountDeleteKey);
            break;

        case kTypeValueExplicitExpiry:
            if (Counters.Value(eSstCountExpiry3)<expires_micros)
                Counters.Set(eSstCountExpiry3, expires_micros);
            // add to delete count if expired already
            //   i.e. acting as a tombstone
            if (MemTableCallback(Key))
                Counters.Inc(eSstCountDeleteKey);
            break;

        // at least one non-expiry, exp_write_low gets zero
        case kTypeValue:
            Counters.Set(eSstCountExpiry1, 0);
            break;

        default:
            break;
    }   // switch

    return(good);

}   // ExpiryModuleOS::TableBuilderCallback


/**
 * Returns true if key is expired.  False if key is NOT expired
 */
bool ExpiryModuleOS::MemTableCallback(
    const Slice & InternalKey) const
{
    bool is_expired(false), good;
    ParsedInternalKey parsed;

    good=ParseInternalKey(InternalKey, &parsed);

    if (good)
        is_expired=KeyRetirementCallback(parsed);

    return(is_expired);

}   // ExpiryModuleOS::MemTableCallback


/**
 * Returns true if at least one file on this level
 *  is eligible for full file expiry
 */
bool ExpiryModuleOS::CompactionFinalizeCallback(
    bool WantAll,                  // input: true - examine all expired files
    const Version & Ver,           // input: database state for examination
    int Level,                     // input: level to review for expiry
    VersionEdit * Edit) const      // output: NULL or destination of delete list
{
    bool ret_flag(false);

    // only test expiry_enable since it is "global" on/off switch.
    //  other parameters might change if bucket level expiry uses
    //  different ExpiryModuleOS object.
    if (IsExpiryEnabled())
    {
        bool expired_file(false);
        ExpiryTimeMicros now_micros;
        const std::vector<FileMetaData*> & files(Ver.GetFileList(Level));
        std::vector<FileMetaData*>::const_iterator it;

        now_micros=GetCachedTimeMicros();
        for (it=files.begin(); (!expired_file || WantAll) && files.end()!=it; ++it)
        {
            // First, is file eligible?
            expired_file=IsFileExpired(*(*it), now_micros);

            // identified an expired file, do any higher levels overlap
            //  its key range?
            if (expired_file)
            {
                int test;
                Slice small, large;

                for (test=Level+1;
                     test<config::kNumLevels && expired_file;
                     ++test)
                {
                    small=(*it)->smallest.user_key();
                    large=(*it)->largest.user_key();
                    expired_file=!Ver.OverlapInLevel(test, &small,
                                                     &large);
                }   // for
                ret_flag=ret_flag || expired_file;
            }   // if

            // expired_file and no overlap? mark it for delete
            if (expired_file && NULL!=Edit)
            {
                Edit->DeleteFile((*it)->level, (*it)->number);
            }   // if
        }   // for
    }   // if

    return(ret_flag);

}   // ExpiryModuleOS::CompactionFinalizeCallback


/**
 * Review the metadata of one file to see if it is
 *  eligible for file expiry
 */
bool
ExpiryModuleOS::IsFileExpired(
    const FileMetaData & SstFile,
    ExpiryTimeMicros NowMicros) const
{
    bool expired_file;
    ExpiryTimeMicros aged_micros;

    aged_micros=NowMicros - GetExpiryMinutes()*60*port::UINT64_ONE_SECOND_MICROS;

    // must test whole_file_expiry here since this could be
    //  a bucket's ExpiryModuleOS object, not the default in Options
    expired_file = (IsExpiryEnabled() && IsWholeFileExpiryEnabled());

    //  - if exp_write_low is zero, game over -  contains non-expiry records
    //  - if exp_write_high is below current aged time and aging enabled,
    //       or no exp_write_high keys (is zero)
    //  - highest explicit expiry (exp_explicit_high) is non-zero and below now
    //  Note:  say file only contained deleted records:  ... still delete file
    //      exp_write_low would be ULLONG_MAX, exp_write_high would be 0, exp_explicit_high would be zero
    expired_file = expired_file && (0!=SstFile.exp_write_low)
                   && (0!=SstFile.exp_write_high || 0!=SstFile.exp_explicit_high);
    expired_file = expired_file && ((SstFile.exp_write_high<=aged_micros
                                     && 0!=GetExpiryMinutes() && !IsExpiryUnlimited())
                                    || 0==SstFile.exp_write_high);

    expired_file = expired_file && (0==SstFile.exp_explicit_high
                                    || (0!=SstFile.exp_explicit_high
                                        && SstFile.exp_explicit_high<=NowMicros));

    return(expired_file);

}   // ExpiryModuleOS::IsFileExpired


/**
 * Riak specific routine to process whole file expiry.
 *  Code here derived from DBImpl::CompactMemTable() in db/db_impl.cc
 */
Status
DBImpl::BackgroundExpiry(
    Compaction * Compact)
{
    Status s;
    size_t count;

    mutex_.AssertHeld();
    assert(NULL != Compact && NULL!=options_.expiry_module.get());
    assert(NULL != Compact->version());

    if (NULL!=Compact && options_.ExpiryActivated())
    {
        VersionEdit edit;
        int level(Compact->level());

        // Compact holds a reference count to version()/input_version_
        const Version* base = Compact->version();
        options_.expiry_module->CompactionFinalizeCallback(true, *base, level,
                                                           &edit);
        count=edit.DeletedFileCount();

        if (s.ok() && shutting_down_.Acquire_Load()) {
            s = Status::IOError("Deleting DB during expiry compaction");
        }

        // push expired list to manifest
        if (s.ok() && 0!=count)
        {
            s = versions_->LogAndApply(&edit, &mutex_);
            if (s.ok())
                gPerfCounters->Add(ePerfExpiredFiles, count);
            else
                s = Status::IOError("LogAndApply error during expiry compaction");
        }   // if

        // Commit to the new state
        if (s.ok() && 0!=count)
        {
            // get rid of Compact now to potential free
            //  input version's files
            delete Compact;
            Compact=NULL;

            DeleteObsoleteFiles();

            // release mutex when writing to log file
            mutex_.Unlock();

            Log(options_.info_log,
                "Expired: %zd files from level %d",
                count, level);
            mutex_.Lock();
        }   // if
    }   // if

    // convention in BackgroundCompaction() is to delete Compact here
    delete Compact;

    return s;

}   // DBImpl:BackgroundExpiry


}  // namespace leveldb
