// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// File names used by DB code

#ifndef STORAGE_LEVELDB_DB_FILENAME_H_
#define STORAGE_LEVELDB_DB_FILENAME_H_

#include <stdint.h>
#include <string>
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"

namespace leveldb {

class Env;
class Version;

enum FileType {
  kLogFile,
  kDBLockFile,
  kTableFile,
  kDescriptorFile,
  kCurrentFile,
  kTempFile,
  kInfoLogFile,  // Either the current one, or an old one
  kCacheWarming
};

// Riak specific routine to help create sst_? subdirectory names
std::string MakeDirName2(const Options & options,
                         int level, const char* suffix);

// Riak specific routine to help create sst_? subdirectories
Status MakeLevelDirectories(Env * env, const Options & options);

// Riak specific routine to test if sst_? subdirectories exist
bool TestForLevelDirectories(Env * env, const Options & options, class Version *);

// Riak specific routine to standardize conversion of dbname and
//  Options' tiered directories (options parameter is MODIFIED)
std::string MakeTieredDbname(const std::string &dbname, Options & options_rw);

// Return the name of the log file with the specified number
// in the db named by "dbname".  The result will be prefixed with
// "dbname".
extern std::string LogFileName(const std::string& dbname, uint64_t number);

// Return the name of the sstable with the specified number
// in the db named by "dbname".  The result will be prefixed with
// "dbname".
extern std::string TableFileName(const Options & options, uint64_t number,
                                 int level);

// Return the name of the descriptor file for the db named by
// "dbname" and the specified incarnation number.  The result will be
// prefixed with "dbname".
extern std::string DescriptorFileName(const std::string& dbname,
                                      uint64_t number);

// Return the name of the current file.  This file contains the name
// of the current manifest file.  The result will be prefixed with
// "dbname".
extern std::string CurrentFileName(const std::string& dbname);

// Return the name of the lock file for the db named by
// "dbname".  The result will be prefixed with "dbname".
extern std::string LockFileName(const std::string& dbname);

// Return the name of a temporary file owned by the db named "dbname".
// The result will be prefixed with "dbname".
extern std::string TempFileName(const std::string& dbname, uint64_t number);

// Return the name of the info log file for "dbname".
extern std::string InfoLogFileName(const std::string& dbname);

// Return the name of the old info log file for "dbname".
extern std::string OldInfoLogFileName(const std::string& dbname);

// Return the name of the cache object file for the db named by
// "dbname".  The result will be prefixed with "dbname".
extern std::string CowFileName(const std::string& dbname);

// Append appropriate "backup" string to input path
extern std::string BackupPath(const std::string& dbname, int backup_num);

// update tiered_fast_prefix and tiered_slow_prefix members of
//  given Options object to point to backup path
extern bool SetBackupPaths(Options & options, int backup_num);

// If filename is a leveldb file, store the type of the file in *type.
// The number encoded in the filename is stored in *number.  If the
// filename was successfully parsed, returns true.  Else return false.
extern bool ParseFileName(const std::string& tiered_filename,
                          uint64_t* number,
                          FileType* type);

// Make the CURRENT file point to the descriptor file with the
// specified number.
extern Status SetCurrentFile(Env* env, const std::string& dbname,
                             uint64_t descriptor_number);


}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_FILENAME_H_
