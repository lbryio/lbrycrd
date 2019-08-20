leveldb: A key-value store
Authors: Sanjay Ghemawat (sanjay@google.com) and Jeff Dean (jeff@google.com)

The original Google README is now README.GOOGLE.

** Introduction

This repository contains the Google source code as modified to benefit
the Riak environment.  The typical Riak environment has two attributes
that necessitate leveldb adjustments, both in options and code:

- production servers: Riak often runs in heavy Internet environments:
  servers with many CPU cores, lots of memory, and 24x7 disk activity.
  Basho's leveldb takes advantage of the environment by adding
  hardware CRC calculation, increasing Bloom filter accuracy, and
  defaulting to integrity checking enabled.

- multiple databases open: Riak opens 8 to 128 databases
  simultaneously.  Google's leveldb supports this, but its background
  compaction thread can fall behind.  leveldb will "stall" new user
  writes whenever the compaction thread gets too far behind.  Basho's
  leveldb modification include multiple thread blocks that each
  contain prioritized threads for specific compaction activities.

Details for Basho's customizations exist in the leveldb wiki:

  http://github.com/basho/leveldb/wiki


** Branch pattern

This repository follows the Basho standard for branch management 
as of November 28, 2013.  The standard is found here:

https://github.com/basho/riak/wiki/Basho-repository-management

In summary, the "develop" branch contains the most recently reviewed
engineering work.  The "master" branch contains the most recently
released work, i.e. distributed as part of a Riak release.


** Basic options needed

Those wishing to truly savor the benefits of Basho's modifications
need to initialize a new leveldb::Options structure similar to the
following before each call to leveldb::DB::Open:

    leveldb::Options * options;

    options=new Leveldb::Options;

    options.filter_policy=leveldb::NewBloomFilterPolicy2(16);
    options.write_buffer_size=62914560;  // 60Mbytes
    options.total_leveldb_mem=2684354560; // 2.5Gbytes (details below)
    options.env=leveldb::Env::Default();


** Memory plan

Basho's leveldb dramatically departed from Google's original internal
memory allotment plan with Riak 2.0.  Basho's leveldb uses a methodology
called flexcache.  The technical details are here:

   https://github.com/basho/leveldb/wiki/mv-flexcache

The key points are:

- options.total_leveldb_mem is an allocation for the entire process,
  not a single database

- giving different values to options.total_leveldb_mem on subsequent Open
  calls causes memory to rearrange to current value across all databases

- recommended minimum for Basho's leveldb is 340Mbytes per database.  

- performance improves rapidly from 340Mbytes to 2.5Gbytes per database (3.0Gbytes
  if using Riak's active anti-entropy).  Even more is nice, but not as helpful.

- never assign more than 75% of available RAM to total_leveldb_mem.  There is
  too much unaccounted memory overhead (worse if you use tcmalloc library).

- options.max_open_files and options.block_cache should not be used.
  
