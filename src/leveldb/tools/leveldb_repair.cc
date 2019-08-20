#include <stdlib.h>
#include <libgen.h>

#include "db/filename.h"
#include "leveldb/env.h"
#include "leveldb/db.h"
#include "leveldb/cache.h"
#include "leveldb/iterator.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "table/format.h"
#include "table/block.h"
#include "table/filter_block.h"

//#include "util/logging.h"
//#include "db/log_reader.h"

void command_help();

int
main(
    int argc,
    char ** argv)
{
    bool error_seen, running;
    int error_counter;
    char ** cursor;

    running=true;
    error_seen=false;
    error_counter=0;


    for (cursor=argv+1; NULL!=*cursor && running; ++cursor)
    {
        // option flag?
        if ('-'==**cursor)
        {
            char flag;

            flag=*((*cursor)+1);
            switch(flag)
            {
                default:
                    fprintf(stderr, " option \'%c\' is not valid\n", flag);
                    command_help();
                    running=false;
                    error_counter+=1;
                    error_seen=true;
                    break;
            }   // switch
        }   // if

        // database path
        else
        {
            std::string dbname;
            leveldb::Options options;
            leveldb::Status status;

            dbname=*cursor;
            options.env=leveldb::Env::Default();

            status=leveldb::RepairDB(dbname.c_str(), options);
            printf("Repair of %s %s.\n",
                   dbname.c_str(),
                   (status.ok() ? "successful" : "failed"));

            if (!status.ok())
            {
                ++error_counter;
                error_seen=true;
            }   // if
        }   // else
    }   // for

    if (1==argc)
        command_help();

    return( error_seen && 0!=error_counter ? 1 : 0 );

}   // main


void
command_help()
{
    fprintf(stderr, "leveldb_repair [option | data_base]*\n");
    fprintf(stderr, "  options\n");
    fprintf(stderr, "      (none at this time)\n");
}   // command_help

namespace leveldb {


}  // namespace leveldb

