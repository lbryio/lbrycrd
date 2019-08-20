// -------------------------------------------------------------------
//
// flexcache_test.cc
//
// Copyright (c) 2013 Basho Technologies, Inc. All Rights Reserved.
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

#include <string>

#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "leveldb/status.h"
#include "util/db_list.h"
#include "util/testharness.h"

namespace leveldb {

class FlexCacheTest { };

TEST(FlexCacheTest, UserSizing) {
    Options options;
    DB * db[10];
    Status st;
    std::string dbname, value;
    int loop;
    char buffer[12];

    options.create_if_missing=true;
    options.filter_policy=NewBloomFilterPolicy2(16);
    options.total_leveldb_mem=1000*1024*1024L;
    options.write_buffer_size=45*1024*1024L;

    // verify accounting with one database
    dbname = test::TmpDir() + "/flexcache0";
    st=DB::Open(options, dbname, &db[0]);
    ASSERT_OK(st);
    ASSERT_EQ(1, DBList()->GetDBCount(false));

    db[0]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(922742784L, atoi(value.c_str()));

    db[0]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(920645632L, atoi(value.c_str()));

    // verify accounting with three databases
    dbname = test::TmpDir() + "/flexcache1";
    st=DB::Open(options, dbname, &db[1]);
    ASSERT_OK(st);
    dbname = test::TmpDir() + "/flexcache2";
    st=DB::Open(options, dbname, &db[2]);
    ASSERT_OK(st);
    ASSERT_EQ(3, DBList()->GetDBCount(false));

    db[0]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(223692117L, atoi(value.c_str()));

    db[0]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(221594965L, atoi(value.c_str()));

    db[1]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(223692117L, atoi(value.c_str()));

    db[1]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(221594965L, atoi(value.c_str()));

    db[2]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(223692117L, atoi(value.c_str()));

    db[2]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(221594965L, atoi(value.c_str()));

    // verify accounting after two databases go away
    delete db[0];
    delete db[2];

    db[1]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(922742784L, atoi(value.c_str()));

    db[1]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(920645632L, atoi(value.c_str()));

    // rebuild from zero to ten databases, verify accounting
    delete db[1];

    options.total_leveldb_mem=3000*1024*1024L;
    for(loop=0; loop<10; ++loop)
    {
        snprintf(buffer, sizeof(buffer), "/flexcache%u", loop);
        dbname=test::TmpDir() + buffer;
        st=DB::Open(options, dbname, &db[loop]);
        ASSERT_OK(st);
        ASSERT_EQ(loop+1, DBList()->GetDBCount(false));
    }   // for

    for(loop=0; loop<10; ++loop)
    {
        db[loop]->GetProperty("leveldb.block-cache", &value);
        ASSERT_EQ(188739584l, atoi(value.c_str()));

        db[loop]->GetProperty("leveldb.file-cache", &value);
        ASSERT_EQ(186642432L, atoi(value.c_str()));
    }   // for

    for (loop=0; loop<10; ++loop)
    {
        delete db[loop];
        snprintf(buffer, sizeof(buffer), "/flexcache%u", loop);
        dbname=test::TmpDir() + buffer;
        st=DestroyDB(dbname, options);
        ASSERT_OK(st);
    }   // for

    delete options.filter_policy;
    options.filter_policy=NULL;
}

TEST(FlexCacheTest, MixedSizing) {
    Options options;
    DB * db[10];
    Status st;
    std::string dbname, value;
    int loop;
    char buffer[12];

    options.create_if_missing=true;
    options.filter_policy=NewBloomFilterPolicy2(16);
    options.total_leveldb_mem=1000*1024*1024L;
    options.write_buffer_size=45*1024*1024L;

    // verify accounting with one user & one internal
    dbname = test::TmpDir() + "/flexcache0";
    st=DB::Open(options, dbname, &db[0]);
    ASSERT_OK(st);
    ASSERT_EQ(1, DBList()->GetDBCount(false));
    ASSERT_EQ(0, DBList()->GetDBCount(true));

    db[0]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(922742784l, atoi(value.c_str()));

    db[0]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(920645632L, atoi(value.c_str()));

    // add internal
    dbname = test::TmpDir() + "/flexcache1";
    options.is_internal_db=true;
    options.total_leveldb_mem=1600*1024*1024L;
    st=DB::Open(options, dbname, &db[1]);
    ASSERT_OK(st);
    ASSERT_EQ(1, DBList()->GetDBCount(false));
    ASSERT_EQ(1, DBList()->GetDBCount(true));

    db[0]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(1216344064l, atoi(value.c_str()));

    db[0]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(1214246912L, atoi(value.c_str()));

    db[1]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(209711104l, atoi(value.c_str()));

    db[1]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(207613952L, atoi(value.c_str()));

    delete db[0];
    ASSERT_EQ(0, DBList()->GetDBCount(false));
    ASSERT_EQ(1, DBList()->GetDBCount(true));
    db[1]->GetProperty("leveldb.block-cache", &value);
    ASSERT_EQ(209711104L, atoi(value.c_str()));

    db[1]->GetProperty("leveldb.file-cache", &value);
    ASSERT_EQ(207613952L, atoi(value.c_str()));

    delete db[1];


    // rebuild from zero to ten databases, verify accounting
    options.total_leveldb_mem=4000*1024*1024L;

    for(loop=0; loop<10; ++loop)
    {
        options.is_internal_db=(1==(loop %2));
        snprintf(buffer, sizeof(buffer), "/flexcache%u", loop);
        dbname=test::TmpDir() + buffer;
        st=DB::Open(options, dbname, &db[loop]);
        ASSERT_OK(st);
    }   // for

    ASSERT_EQ(5, DBList()->GetDBCount(false));
    ASSERT_EQ(5, DBList()->GetDBCount(true));

    for(loop=0; loop<10; ++loop)
    {
        if (0==(loop %2))
        {
            db[loop]->GetProperty("leveldb.block-cache", &value);
            ASSERT_EQ(545255424l, atoi(value.c_str()));

            db[loop]->GetProperty("leveldb.file-cache", &value);
            ASSERT_EQ(543158272L, atoi(value.c_str()));
        }   // if
        else
        {
            db[loop]->GetProperty("leveldb.block-cache", &value);
            ASSERT_EQ(41938944l, atoi(value.c_str()));

            db[loop]->GetProperty("leveldb.file-cache", &value);
            ASSERT_EQ(39841792L, atoi(value.c_str()));
        }   // else
    }   // for

    for (loop=0; loop<10; ++loop)
    {
        delete db[loop];
        snprintf(buffer, sizeof(buffer), "/flexcache%u", loop);
        dbname=test::TmpDir() + buffer;
        st=DestroyDB(dbname, options);
        ASSERT_OK(st);
    }   // for

    delete options.filter_policy;
    options.filter_policy=NULL;
}


}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
