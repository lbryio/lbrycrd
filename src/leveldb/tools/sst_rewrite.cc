// -------------------------------------------------------------------
//
// sst_rewrite.cc
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

#include <memory>
#include <stdio.h>
#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "leveldb/table.h"
#include "leveldb/table_builder.h"

void command_help();

// wrapper class for opening / closing existing leveldb tables
class LDbTable
{
public:
    LDbTable(leveldb::Options &, std::string &);
    virtual ~LDbTable();

    bool Ok() const {return(m_IsOpen);};
    leveldb::Iterator * NewIterator();

    const leveldb::Status & GetStatus() const {return(m_LastStatus);};
    const char * GetFileName() const {return(m_FileName.c_str());};

    uint64_t GetSstCounter(unsigned Idx) const
        {return(m_IsOpen ? m_TablePtr->GetSstCounters().Value(Idx) : 0);};

protected:
    leveldb::Options & m_Options;
    const std::string m_FileName;
    leveldb::RandomAccessFile * m_FilePtr;
    leveldb::Table * m_TablePtr;
    uint64_t m_FileSize;
    leveldb::Status m_LastStatus;

    bool m_IsOpen;

    void Reset();

private:
    // disable these
    LDbTable();
    LDbTable(const LDbTable &);
    const LDbTable operator=(const LDbTable&);
};  // LDbTable


LDbTable::LDbTable(
    leveldb::Options & Options,
    std::string & FileName)
    : m_Options(Options), m_FileName(FileName),
      m_FilePtr(NULL), m_TablePtr(NULL), m_FileSize(0), m_IsOpen(false)
{
    m_LastStatus=m_Options.env->GetFileSize(m_FileName, &m_FileSize);

    if (m_LastStatus.ok())
        {m_LastStatus=m_Options.env->NewRandomAccessFile(m_FileName, &m_FilePtr);}

    if (m_LastStatus.ok())
    {
        m_LastStatus=leveldb::Table::Open(m_Options, m_FilePtr, m_FileSize, &m_TablePtr);

        // use fadvise to start file pre-read
        m_FilePtr->SetForCompaction(m_FileSize);
    }   // if

    m_IsOpen=m_LastStatus.ok();

    if (!m_IsOpen)
    {
        // some people would throw() at this point, but not me
        Reset();
    }   // if

    return;

}   // LDbTable::LDbTable


LDbTable::~LDbTable()
{
    Reset();

    return;

}   // LDbTable::~LDbTable


void
LDbTable::Reset()
{
    m_IsOpen=false;
    delete m_TablePtr;
    m_TablePtr=NULL;
    delete m_FilePtr;
    m_FilePtr=NULL;
    m_FileSize=0;

    return;

}   // LDbTable::Reset


leveldb::Iterator *
LDbTable::NewIterator()
{
    leveldb::Iterator * ret_ptr(NULL);

    if (m_IsOpen)
    {
        leveldb::ReadOptions read_options;

        read_options.fill_cache=false;
        ret_ptr=m_TablePtr->NewIterator(read_options);
    }   // if

    return(ret_ptr);

}   // LDbTable::NewIterator


int
main(
    int argc,
    char ** argv)
{
    bool error_seen, running, compare_files;
    char ** cursor;

    compare_files=false;
    error_seen=false;
    running=true;

    // Options: needs filter & total_leveldb_mem initialized
    leveldb::Options options;

    // using 16 bit width per key in bloom filter
    options.filter_policy=leveldb::NewBloomFilterPolicy2(16);
    // tell leveldb it can use 512Mbyte of memory
    options.total_leveldb_mem=(512 << 20);

    for (cursor=argv+1;
         NULL!=*cursor && running && !error_seen;
         ++cursor)
    {
        // option flag?
        if ('-'==**cursor)
        {
            char flag;

            flag=*((*cursor)+1);
            switch(flag)
            {
                case 'b':
                {
                    error_seen=(NULL==(cursor+1));
                    if (!error_seen)
                    {
                        ++cursor;
                        options.block_size=atol(*cursor);
                    };
                    break;
                }   // case b

                case 's':  options.compression=leveldb::kSnappyCompression; break;
                case 'z':  options.compression=leveldb::kLZ4Compression; break;
                case 'n':  options.compression=leveldb::kNoCompression; break;

                case 'c':
                {
                    // test for first pair ... but after that user beware
                    error_seen=(NULL==(cursor+1)) || (NULL==(cursor+2));
                    if (!error_seen)
                         {compare_files=true;}
                    break;
                }   // case c

                case 'w':  compare_files=false; break;

                default:
                    fprintf(stderr, " option \'%c\' is not valid\n", flag);
                    command_help();
                    running=false;
                    error_seen=true;
                    break;
            }   // switch
        }   // if

        // sst file
        else
        {
            std::string fname;
            fname=*cursor;

            // do a rewrite
            if (!compare_files)
            {
                leveldb::WritableFile * outfile;
                leveldb::Status s;
                std::auto_ptr<leveldb::Iterator> it;
                std::auto_ptr<leveldb::TableBuilder> builder;

                LDbTable in_file(options, fname);

                if (in_file.GetStatus().ok())
                {
                    it.reset(in_file.NewIterator());

                    fname.append(".new");
                    s = options.env->NewWritableFile(fname, &outfile,
                                                     options.env->RecoveryMmapSize(&options));
                    if (s.ok())
                        builder.reset(new leveldb::TableBuilder(options, outfile));
                    else
                    {
                        // Table::Open failed on file "fname"
                        fprintf(stderr, "%s: NewWritableFile failed (%s)\n",
                                fname.c_str(), s.ToString().c_str());
                        error_seen=true;
                    }   // else

                    for (it->SeekToFirst();
                         it->Valid() && s.ok() && builder->status().ok();
                         it->Next())
                    {
                        leveldb::Slice key = it->key();
                        builder->Add(key, it->value());
                    }   // for

                    // hmmm, nothing new setting status right now.
                    if (s.ok() && builder->status().ok()) {
                        s = builder->Finish();
                    } else {
                        builder->Abandon();
                    }

                    if (NULL!=outfile)
                        outfile->Close();
                    delete outfile;
                }   // if
                else
                {
                    fprintf(stderr, "%s: Input table open failed (%s)\n",
                            fname.c_str(), in_file.GetStatus().ToString().c_str());
                    error_seen=true;
                }   // else
            }   // if

            // compare two files
            else
            {
                LDbTable file1(options, fname);

                ++cursor;
                if (NULL!=*cursor)
                {
                    fname=*cursor;
                    LDbTable file2(options, fname);

                    if (file1.GetStatus().ok() && file2.GetStatus().ok())
                    {
                        // quick check: same number of keys and bytes of user data?
                        //     do this before reading entire files
                        if (file1.GetSstCounter(leveldb::eSstCountKeys)==file2.GetSstCounter(leveldb::eSstCountKeys)
                            && file1.GetSstCounter(leveldb::eSstCountKeySize)==file2.GetSstCounter(leveldb::eSstCountKeySize)
                            && file1.GetSstCounter(leveldb::eSstCountValueSize)==file2.GetSstCounter(leveldb::eSstCountValueSize))
                        {
                            leveldb::Iterator * it1, *it2;
                            uint64_t key_count;
                            bool match;

                            it1=file1.NewIterator();
                            it2=file2.NewIterator();
                            match=true;

                            for (it1->SeekToFirst(), it2->SeekToFirst(), key_count=1;
                                 it1->Valid() && it2->Valid() && match;
                                 it1->Next(), it2->Next(), ++key_count)
                            {
                                match=(0==it1->key().compare(it2->key())) && (0==it1->value().compare(it2->value()));

                                if (!match)
                                {
                                    fprintf(stderr, "%s, %s: Content mismatch at key position %d (%d, %d).\n",
                                            file1.GetFileName(), file2.GetFileName(),
                                            (int)key_count,
                                            it1->key().compare(it2->key()), it1->value().compare(it2->value()));
                                    error_seen=true;
                                }   // if

                            }   // for

                            if (it1->Valid() != it2->Valid())
                            {
                                fprintf(stderr, "%s, %s: Walk of keys terminated early (%d, %d).\n",
                                        file1.GetFileName(), file2.GetFileName(),
                                        (int)it1->Valid(), (int)it2->Valid());
                                error_seen=true;
                            }
                        }   // if
                        else
                        {
                            if (file1.GetSstCounter(leveldb::eSstCountKeys)==file2.GetSstCounter(leveldb::eSstCountKeys))
                                fprintf(stderr, "%s, %s: Number of keys different, %" PRIu64 " vs %" PRIu64 ".\n",
                                        file1.GetFileName(), file2.GetFileName(),
                                        file1.GetSstCounter(leveldb::eSstCountKeys),
                                        file2.GetSstCounter(leveldb::eSstCountKeys));

                            if (file1.GetSstCounter(leveldb::eSstCountKeySize)==file2.GetSstCounter(leveldb::eSstCountKeySize))
                                fprintf(stderr, "%s, %s: Byte size of all keys different, %" PRIu64 " vs %" PRIu64 "\n",
                                        file1.GetFileName(), file2.GetFileName(),
                                        file1.GetSstCounter(leveldb::eSstCountKeySize),
                                        file2.GetSstCounter(leveldb::eSstCountKeySize));

                            if (file1.GetSstCounter(leveldb::eSstCountValueSize)==file2.GetSstCounter(leveldb::eSstCountValueSize))
                                fprintf(stderr, "%s, %s: Byte size of all values different, %" PRIu64 " vs %" PRIu64 "\n",
                                        file1.GetFileName(), file2.GetFileName(),
                                        file1.GetSstCounter(leveldb::eSstCountValueSize),
                                        file2.GetSstCounter(leveldb::eSstCountValueSize));
                            error_seen=true;
                        }   // else
                    }   // if
                    else
                    {
                        if (!file1.GetStatus().ok())
                            fprintf(stderr, "%s: Input table open failed (%s)\n",
                                    file1.GetFileName(), file1.GetStatus().ToString().c_str());
                        if (!file2.GetStatus().ok())
                            fprintf(stderr, "%s: Input table open failed (%s)\n",
                                    file2.GetFileName(), file2.GetStatus().ToString().c_str());
                        error_seen=true;
                    }   // else
                }   // if
                else
                {
                    fprintf(stderr, "%s: compare needs two file names, only have one\n",
                            fname.c_str());
                }   // else
            }   // else
        }   // else
    }   // for

    // cleanup
    options.env->Shutdown();
    delete options.filter_policy;

    if (1==argc)
        command_help();

    return( error_seen ? 1 : 0 );

}   // main


void
command_help()
{
    fprintf(stderr, "sst_rewrite [option | file]*\n");
    fprintf(stderr, "  options\n");
    fprintf(stderr, "      -b  value  set Options.block_size to value\n");
    fprintf(stderr, "      -n  set Options.compression to No compression\n");
    fprintf(stderr, "      -s  set Options.compression to Snappy compression\n");
    fprintf(stderr, "      -z  set Options.compression to LZ4 compression\n");
    fprintf(stderr, "      -c  compare next two files (inverse of -w)\n");
    fprintf(stderr, "      -w  rewrite next file (default, inverse of -c)\n");
}   // command_help

namespace leveldb {


}  // namespace leveldb

