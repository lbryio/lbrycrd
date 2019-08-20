// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/filename.h"

#include "db/dbformat.h"
#include "port/port.h"
#include "util/logging.h"
#include "util/testharness.h"

namespace leveldb {

class FileNameTest { };

TEST(FileNameTest, Parse) {
  Slice db;
  FileType type;
  uint64_t number;

  // Successful parses
  static struct {
    const char* fname;
    uint64_t number;
    FileType type;
  } cases[] = {
    { "100.log",            100,   kLogFile },
    { "0.log",              0,     kLogFile },
    { "0.sst",              0,     kTableFile },
    { "CURRENT",            0,     kCurrentFile },
    { "LOCK",               0,     kDBLockFile },
    { "MANIFEST-2",         2,     kDescriptorFile },
    { "MANIFEST-7",         7,     kDescriptorFile },
    { "LOG",                0,     kInfoLogFile },
    { "LOG.old",            0,     kInfoLogFile },
    { "18446744073709551615.log", 18446744073709551615ull, kLogFile },
  };
  for (int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    std::string f = cases[i].fname;
    ASSERT_TRUE(ParseFileName(f, &number, &type)) << f;
    ASSERT_EQ(cases[i].type, type) << f;
    ASSERT_EQ(cases[i].number, number) << f;
  }

  // Errors
  static const char* errors[] = {
    "",
    "foo",
    "foo-dx-100.log",
    ".log",
    "",
    "manifest",
    "CURREN",
    "CURRENTX",
    "MANIFES",
    "MANIFEST",
    "MANIFEST-",
    "XMANIFEST-3",
    "MANIFEST-3x",
    "LOC",
    "LOCKx",
    "LO",
    "LOGx",
    "18446744073709551616.log",
    "184467440737095516150.log",
    "100",
    "100.",
    "100.lop"
  };
  for (int i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
    std::string f = errors[i];
    ASSERT_TRUE(!ParseFileName(f, &number, &type)) << f;
  };
}

TEST(FileNameTest, Construction) {
  uint64_t number;
  FileType type;
  std::string fname;
  Options options;

  fname = CurrentFileName("foo");
  ASSERT_EQ("foo/", std::string(fname.data(), 4));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 4, &number, &type));
  ASSERT_EQ(0, number);
  ASSERT_EQ(kCurrentFile, type);

  fname = LockFileName("foo");
  ASSERT_EQ("foo/", std::string(fname.data(), 4));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 4, &number, &type));
  ASSERT_EQ(0, number);
  ASSERT_EQ(kDBLockFile, type);

  fname = LogFileName("foo", 192);
  ASSERT_EQ("foo/", std::string(fname.data(), 4));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 4, &number, &type));
  ASSERT_EQ(192, number);
  ASSERT_EQ(kLogFile, type);

  options.tiered_fast_prefix="bar";
  options.tiered_slow_prefix="bar";
  fname = TableFileName(options, 200, 1);
  ASSERT_EQ("bar/", std::string(fname.data(), 4));
  ASSERT_EQ("sst_1/", std::string(fname.substr(4,6)));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 10, &number, &type));
  ASSERT_EQ(200, number);
  ASSERT_EQ(kTableFile, type);

  fname = TableFileName(options, 400, 4);
  ASSERT_EQ("bar/", std::string(fname.data(), 4));
  ASSERT_EQ("sst_4/", std::string(fname.substr(4,6)));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 10, &number, &type));
  ASSERT_EQ(400, number);
  ASSERT_EQ(kTableFile, type);

  options.tiered_slow_level=4;
  options.tiered_fast_prefix="fast";
  options.tiered_slow_prefix="slow";
  fname = TableFileName(options, 500, 3);
  ASSERT_EQ("fast/", std::string(fname.data(), 5));
  ASSERT_EQ("sst_3/", std::string(fname.substr(5,6)));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 11, &number, &type));
  ASSERT_EQ(500, number);
  ASSERT_EQ(kTableFile, type);

  fname = TableFileName(options, 600, 4);
  ASSERT_EQ("slow/", std::string(fname.data(), 5));
  ASSERT_EQ("sst_4/", std::string(fname.substr(5,6)));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 11, &number, &type));
  ASSERT_EQ(600, number);
  ASSERT_EQ(kTableFile, type);


  fname = DescriptorFileName("bar", 100);
  ASSERT_EQ("bar/", std::string(fname.data(), 4));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 4, &number, &type));
  ASSERT_EQ(100, number);
  ASSERT_EQ(kDescriptorFile, type);

  fname = TempFileName("tmp", 999);
  ASSERT_EQ("tmp/", std::string(fname.data(), 4));
  ASSERT_TRUE(ParseFileName(fname.c_str() + 4, &number, &type));
  ASSERT_EQ(999, number);
  ASSERT_EQ(kTempFile, type);

  fname = CowFileName("/what/goes/moo");
  ASSERT_EQ("/what/goes/moo/COW", fname);

  fname = BackupPath("/var/db/riak/data/leveldb/0",0);
  ASSERT_EQ("/var/db/riak/data/leveldb/0/backup", fname);

  fname = BackupPath("/var/db/riak/data/leveldb/0",1);
  ASSERT_EQ("/var/db/riak/data/leveldb/0/backup.1", fname);

  fname = BackupPath("/var/db/riak/data/leveldb/0",5);
  ASSERT_EQ("/var/db/riak/data/leveldb/0/backup.5", fname);

  options.tiered_slow_level=4;
  options.tiered_fast_prefix="fast";
  options.tiered_slow_prefix="slow";
  fname = SetBackupPaths(options,0);
  ASSERT_EQ("fast/backup", options.tiered_fast_prefix);
  ASSERT_EQ("slow/backup", options.tiered_slow_prefix);

  options.tiered_slow_level=4;
  options.tiered_fast_prefix="fast";
  options.tiered_slow_prefix="slow";
  fname = SetBackupPaths(options,3);
  ASSERT_EQ("fast/backup.3", options.tiered_fast_prefix);
  ASSERT_EQ("slow/backup.3", options.tiered_slow_prefix);


  options.tiered_slow_level=4;
  options.tiered_fast_prefix="//mnt/fast";
  options.tiered_slow_prefix="//mnt/slow";
  fname=MakeTieredDbname("riak/data/leveldb", options);
  ASSERT_EQ("//mnt/fast/riak/data/leveldb", fname);
  ASSERT_EQ("//mnt/fast/riak/data/leveldb", options.tiered_fast_prefix);
  ASSERT_EQ("//mnt/slow/riak/data/leveldb", options.tiered_slow_prefix);

  // special case with no dbname given, should have no changes
  fname=MakeTieredDbname("", options);
  ASSERT_EQ("//mnt/fast/riak/data/leveldb", fname);
  ASSERT_EQ("//mnt/fast/riak/data/leveldb", options.tiered_fast_prefix);
  ASSERT_EQ("//mnt/slow/riak/data/leveldb", options.tiered_slow_prefix);

}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
