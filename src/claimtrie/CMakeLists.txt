cmake_minimum_required(VERSION 3.10)

project(claimtrie)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(../../contrib/cmake/cmake/CPM.cmake)
include(ExternalProject)

set(CLAIMTRIE_SRC
    data.cpp
    forks.cpp
    hashes.cpp
    log.cpp
    prefixtrie.cpp
    trie.cpp
    txoutpoint.cpp
    uints.cpp
)

add_library(claimtrie SHARED ${CLAIMTRIE_SRC})

target_include_directories(claimtrie PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# TODO remove leveldb depends
set(LEVELDB_SRC
    ../leveldb/db/builder.cc
    ../leveldb/db/c.cc
    ../leveldb/db/dbformat.cc
    ../leveldb/db/db_impl.cc
    ../leveldb/db/db_iter.cc
    ../leveldb/db/dumpfile.cc
    ../leveldb/db/filename.cc
    ../leveldb/db/log_reader.cc
    ../leveldb/db/log_writer.cc
    ../leveldb/db/memtable.cc
    ../leveldb/db/repair.cc
    ../leveldb/db/table_cache.cc
    ../leveldb/db/version_edit.cc
    ../leveldb/db/version_set.cc
    ../leveldb/db/write_batch.cc
    ../leveldb/table/block_builder.cc
    ../leveldb/table/block.cc
    ../leveldb/table/filter_block.cc
    ../leveldb/table/format.cc
    ../leveldb/table/iterator.cc
    ../leveldb/table/merger.cc
    ../leveldb/table/table_builder.cc
    ../leveldb/table/table.cc
    ../leveldb/table/two_level_iterator.cc
    ../leveldb/util/arena.cc
    ../leveldb/util/bloom.cc
    ../leveldb/util/cache.cc
    ../leveldb/util/coding.cc
    ../leveldb/util/comparator.cc
    ../leveldb/util/crc32c.cc
    ../leveldb/util/env.cc
    ../leveldb/util/env_posix.cc
    ../leveldb/util/filter_policy.cc
    ../leveldb/util/hash.cc
    ../leveldb/util/histogram.cc
    ../leveldb/util/logging.cc
    ../leveldb/util/options.cc
    ../leveldb/util/status.cc
)

if(WIN32)
    set(LEVELDB_SRC ${LEVELDB_SRC}
        ../leveldb/util/env_win.cc
        ../leveldb/port/port_win.cc
    )
else()
    set(LEVELDB_SRC ${LEVELDB_SRC}
        ../leveldb/port/port_posix.cc
    )
endif()

add_library(leveldb STATIC ${LEVELDB_SRC})

target_include_directories(leveldb PRIVATE ../leveldb ../leveldb/include)

if(WIN32)
    target_compile_definitions(leveldb PRIVATE -DLEVELDB_PLATFORM_WINDOWS -DWINVER=0x0500 -D__USE_MINGW_ANSI_STDIO=1)
else()
    target_compile_definitions(leveldb PRIVATE -DLEVELDB_PLATFORM_POSIX)
endif()

target_compile_definitions(leveldb PRIVATE -DLEVELDB_ATOMIC_PRESENT -D__STDC_LIMIT_MACROS)

target_link_libraries(claimtrie PRIVATE leveldb)
# leveldb depends end here

# TODO remove BITCOIN depends
target_include_directories(claimtrie PRIVATE ../ ../leveldb/include)
# it's needed a configure call to create config files
target_compile_definitions(claimtrie PRIVATE -DHAVE_CONFIG_H)
# bitcoin depends end here

CPMAddPackage(
    NAME OpenSSL
    GITHUB_REPOSITORY openssl/openssl
    VERSION 1.0.2
    GIT_TAG OpenSSL_1_0_2r
    DOWNLOAD_ONLY TRUE
)

if(OpenSSL_ADDED)
    string(TOLOWER ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR} ARCH)
    ExternalProject_Add(OpenSSL
        PREFIX openssl
        SOURCE_DIR ${OpenSSL_SOURCE_DIR}
        CONFIGURE_COMMAND ${OpenSSL_SOURCE_DIR}/Configure ${ARCH} no-shared no-dso no-engines -fPIC --prefix=<INSTALL_DIR>
        BUILD_IN_SOURCE 1
    )
    add_dependencies(claimtrie OpenSSL)
    ExternalProject_Get_Property(OpenSSL INSTALL_DIR)
    target_link_directories(claimtrie PRIVATE ${INSTALL_DIR}/lib)
    target_include_directories(claimtrie PRIVATE ${INSTALL_DIR}/include)
endif(OpenSSL_ADDED)

target_link_libraries(claimtrie PRIVATE ssl)

set(BOOST_LIBS filesystem,locale,system,chrono,thread,test)

set(BOOST_COMPONENTS filesystem;locale;system,chrono,thread,unit_test_framework)

CPMAddPackage(
    NAME Boost
    GITHUB_REPOSITORY boostorg/boost
    VERSION 1.64.0
    COMPONENTS ${BOOST_COMPONENTS}
    GIT_TAG boost-1.69.0
    GIT_SUBMODULES libs/* tools/*
    DOWNLOAD_ONLY TRUE
)

# if boost is found system wide we expect to be compiled against icu, so we can skip it
if(Boost_ADDED)
    CPMAddPackage(
        NAME ICU
        GITHUB_REPOSITORY unicode-org/icu
        VERSION 63.2
        GIT_TAG release-63-2
        DOWNLOAD_ONLY TRUE
    )

    if(ICU_ADDED)
        ExternalProject_Add(ICU
            PREFIX icu
            SOURCE_DIR ${ICU_SOURCE_DIR}
            CONFIGURE_COMMAND ${ICU_SOURCE_DIR}/icu4c/source/configure --disable-extras --disable-strict --enable-static
                --disable-shared --disable-tests --disable-samples --disable-dyload --disable-layoutex CFLAGS=-fPIC CPPFLAGS=-fPIC --prefix=<INSTALL_DIR>
        )
        ExternalProject_Get_Property(ICU INSTALL_DIR)
        set(ICU_PATH ${INSTALL_DIR})
        target_link_directories(claimtrie PRIVATE ${ICU_PATH}/lib)
        target_include_directories(claimtrie PRIVATE ${ICU_PATH}/include)
    endif(ICU_ADDED)

    ExternalProject_Add(Boost
        PREFIX boost
        DEPENDS ICU
        SOURCE_DIR ${Boost_SOURCE_DIR}
        CONFIGURE_COMMAND ${Boost_SOURCE_DIR}/bootstrap.sh --with-icu=${ICU_PATH} --with-libraries=${BOOST_LIBS} && ${Boost_SOURCE_DIR}/b2 headers
        BUILD_COMMAND ${Boost_SOURCE_DIR}/b2 install threading=multi -sNO_BZIP2=1 -sNO_ZLIB=1 link=static linkflags="-L${ICU_PATH}/lib -licuio -licuuc -licudata -licui18n" cxxflags=-fPIC boost.locale.iconv=off boost.locale.posix=off boost.locale.icu=on boost.locale.std=off -sICU_PATH=${ICU_PATH} --prefix=<INSTALL_DIR>
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
    )
    add_dependencies(claimtrie Boost)
    ExternalProject_Get_Property(Boost INSTALL_DIR)
    target_link_directories(claimtrie PRIVATE ${INSTALL_DIR}/lib)
    target_include_directories(claimtrie PRIVATE ${INSTALL_DIR}/include)
    set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${Boost_SOURCE_DIR}/bin.v2)
endif(Boost_ADDED)

target_link_libraries(claimtrie PRIVATE boost_filesystem boost_locale)
