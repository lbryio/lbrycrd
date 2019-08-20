// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "db/filename.h"
#include "db/dbformat.h"
#include "db/version_set.h"
#include "leveldb/env.h"
#include "leveldb/status.h"
#include "util/logging.h"

namespace leveldb {

// A utility routine: write "data" to the named file and Sync() it.
extern Status WriteStringToFileSync(Env* env, const Slice& data,
                                    const std::string& fname);

static std::string MakeFileName(const std::string& name, uint64_t number,
                                const char* suffix) {
  char buf[100];
  snprintf(buf, sizeof(buf), "/%06llu.%s",
           static_cast<unsigned long long>(number),
           suffix);
  return name + buf;
}

static std::string MakeFileName2(const Options & options, uint64_t number,
                                 int level, const char* suffix) {
  char buf[100];
  if (0<=level)
      snprintf(buf, sizeof(buf), "/%s_%-d/%06llu.%s",
               suffix, level,
               static_cast<unsigned long long>(number),
               suffix);
  else if (-1==level)
      snprintf(buf, sizeof(buf), "/%s/%06llu.%s",
               suffix,
               static_cast<unsigned long long>(number),
               suffix);
  else if (-2==level)
      snprintf(buf, sizeof(buf), "/%06llu.%s",
               static_cast<unsigned long long>(number),
               suffix);

  return((level<(int)options.tiered_slow_level ?
          options.tiered_fast_prefix : options.tiered_slow_prefix) + buf);
}

std::string MakeDirName2(const Options & options,
                         int level, const char* suffix) {
  char buf[100];
  if (-1!=level)
      snprintf(buf, sizeof(buf), "/%s_%-d",
               suffix, level);
  else
      snprintf(buf, sizeof(buf), "/%s",
               suffix);

  return((level<(int)options.tiered_slow_level ?
          options.tiered_fast_prefix : options.tiered_slow_prefix) + buf);
}

std::string LogFileName(const std::string& name, uint64_t number) {
  assert(number > 0);
  return MakeFileName(name, number, "log");
}

std::string TableFileName(const Options & options, uint64_t number, int level) {
  assert(number > 0);
  return MakeFileName2(options, number, level, "sst");
}

std::string DescriptorFileName(const std::string& dbname, uint64_t number) {
  assert(number > 0);
  char buf[100];
  snprintf(buf, sizeof(buf), "/MANIFEST-%06llu",
           static_cast<unsigned long long>(number));
  return dbname + buf;
}

std::string CurrentFileName(const std::string& dbname) {
  return dbname + "/CURRENT";
}

std::string LockFileName(const std::string& dbname) {
  return dbname + "/LOCK";
}

std::string TempFileName(const std::string& dbname, uint64_t number) {
  assert(number > 0);
  return MakeFileName(dbname, number, "dbtmp");
}

std::string InfoLogFileName(const std::string& dbname) {
  return dbname + "/LOG";
}

// Return the name of the old info log file for "dbname".
std::string OldInfoLogFileName(const std::string& dbname) {
  return dbname + "/LOG.old";
}

//
std::string CowFileName(const std::string& dbname) {
  return dbname + "/COW";
}


// Append appropriate "backup" string to input path
std::string BackupPath(const std::string& dbname, int backup_num) {
    std::string dirname;

  char buf[100];
  if (0 != backup_num)
      snprintf(buf, sizeof(buf), "/backup.%-d", backup_num);
  else
      snprintf(buf, sizeof(buf), "/backup");

  return(dbname + buf);
}


// update tiered_fast_prefix and tiered_slow_prefix members of
//  given Options object to point to desired backup path
bool SetBackupPaths(Options & options, int backup_num) {

    options.tiered_fast_prefix = BackupPath(options.tiered_fast_prefix, backup_num);
    options.tiered_slow_prefix = BackupPath(options.tiered_slow_prefix, backup_num);

    return(true);
}


// Owned filenames have the form:
//    dbname/CURRENT
//    dbname/LOCK
//    dbname/LOG
//    dbname/LOG.old
//    dbname/MANIFEST-[0-9]+
//    dbname/[0-9]+.(log|sst)
//    dbname/COW
bool ParseFileName(const std::string& fname,
                   uint64_t* number,
                   FileType* type) {
  Slice rest(fname);
  if (rest == "CURRENT") {
    *number = 0;
    *type = kCurrentFile;
  } else if (rest == "COW") {
    *number = 0;
    *type = kCacheWarming;
  } else if (rest == "LOCK") {
    *number = 0;
    *type = kDBLockFile;
  } else if (rest == "LOG" || rest == "LOG.old") {
    *number = 0;
    *type = kInfoLogFile;
  } else if (rest.starts_with("MANIFEST-")) {
    rest.remove_prefix(strlen("MANIFEST-"));
    uint64_t num;
    if (!ConsumeDecimalNumber(&rest, &num)) {
      return false;
    }
    if (!rest.empty()) {
      return false;
    }
    *type = kDescriptorFile;
    *number = num;
  } else {
    // Avoid strtoull() to keep filename format independent of the
    // current locale
    uint64_t num;
    if (!ConsumeDecimalNumber(&rest, &num)) {
      return false;
    }
    Slice suffix = rest;
    if (suffix == Slice(".log")) {
      *type = kLogFile;
    } else if (suffix == Slice(".sst")) {
      *type = kTableFile;
    } else if (suffix == Slice(".dbtmp")) {
      *type = kTempFile;
    } else {
      return false;
    }
    *number = num;
  }
  return true;
}

Status SetCurrentFile(Env* env, const std::string& dbname,
                      uint64_t descriptor_number) {
  // Remove leading "dbname/" and add newline to manifest file name
  std::string manifest = DescriptorFileName(dbname, descriptor_number);
  Slice contents = manifest;
  assert(contents.starts_with(dbname + "/"));
  contents.remove_prefix(dbname.size() + 1);
  std::string tmp = TempFileName(dbname, descriptor_number);
  Status s = WriteStringToFileSync(env, contents.ToString() + "\n", tmp);
  if (s.ok()) {
    s = env->RenameFile(tmp, CurrentFileName(dbname));
  }
  if (!s.ok()) {
    env->DeleteFile(tmp);
  }
  return s;
}


Status
MakeLevelDirectories(Env * env, const Options & options)
{
    Status ret_stat;
    int level;
    std::string dirname;

    for (level=0; level<config::kNumLevels && ret_stat.ok(); ++level)
    {
        dirname=MakeDirName2(options, level, "sst");

        // ignoring error since no way to tell if "bad" error, or "already exists" error
        env->CreateDir(dirname.c_str());
    }   // for

    return(ret_stat);

}  // MakeLevelDirectories


bool
TestForLevelDirectories(
    Env * env,
    const Options & options,
    Version * version)
{
    bool ret_flag, again;
    int level;
    std::string dirname;

    ret_flag=true;
    again=true;

    // walk backwards, fault will be in higher levels if partial conversion
    for (level=config::kNumLevels-1; 0<=level && again; --level)
    {
        again=false;

        // does directory exist
        dirname=MakeDirName2(options, level, "sst");
        ret_flag=env->FileExists(dirname.c_str());

        // do all files exist in level
        if (ret_flag)
        {
            const std::vector<FileMetaData*> & level_files(version->GetFileList(level));
            std::vector<FileMetaData*>::const_iterator it;
            std::string table_name;
            Status s;

            for (it=level_files.begin(); level_files.end()!=it && ret_flag; ++it)
            {
                table_name=TableFileName(options, (*it)->number, level);
                ret_flag=env->FileExists(table_name.c_str());
            }   // for

            again=ret_flag && 0==level_files.size();
        }   // if
    }   // for

    return(ret_flag);

}   // TestForLevelDirectories

std::string       // replacement dbname ... potentially tiered
MakeTieredDbname(
    const std::string & dbname,    // input ... original dbname from DBImpl constructor
    Options & options)             // input/output ... writable Options, tiered values changed
{
    // case for "", used with internal calls to DestroyDB
    if (0==dbname.size() && 0!=options.tiered_fast_prefix.size())
    {
        // do NOTHING ... options already initialized
    }   // if
    else if (0<(int)options.tiered_slow_level && (int)options.tiered_slow_level<config::kNumLevels
        && 0!=options.tiered_fast_prefix.size() && 0!=options.tiered_slow_prefix.size())
    {
        options.tiered_fast_prefix.append("/");
        options.tiered_fast_prefix.append(dbname);

        options.tiered_slow_prefix.append("/");
        options.tiered_slow_prefix.append(dbname);
    }   // else if
    else
    {
        options.tiered_slow_level=0;
        options.tiered_fast_prefix=dbname; // duplicate as is
        options.tiered_slow_prefix=dbname;
    }   // else

    return(options.tiered_fast_prefix);

}   // MakeTieredDbname

}  // namespace leveldb
