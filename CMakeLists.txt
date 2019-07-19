cmake_minimum_required(VERSION 3.10)

project(lbrycrd)
set(CMAKE_CXX_STANDARD 11)

include(cmake/CPM.cmake)
include(ExternalProject)

set(OPTIONS "" CACHE STRING "lbrycrdd configure options")
set(CPPFLAGS "" CACHE STRING "lbrycrdd compiler options")
set(LDFLAGS "" CACHE STRING "lbrycrdd linker options")
set(DISABLE_TESTS OFF CACHE BOOL "compilation without tests")
set(DISABLE_WALLET OFF CACHE BOOL "compilation without wallet support")
set(DISABLE_BENCH OFF CACHE BOOL "compilation without bench support")

if(NOT ${CPM_USE_LOCAL_PACKAGES})
    set(OPTIONS "${OPTIONS} --enable-static --disable-shared")
else()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
endif()

set(OPTIONS "--without-gui ${OPTIONS} --with-pic")

if (${DISABLE_TESTS})
    set(OPTIONS "${OPTIONS} --disable-tests")
endif()

if (${DISABLE_WALLET})
    set(OPTIONS "${OPTIONS} --disable-wallet")
endif()

if (${DISABLE_BENCH})
    set(OPTIONS "${OPTIONS} --disable-bench")
endif()

string(TOLOWER ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR} ARCH)

CPMAddPackage(
    NAME OpenSSL
    GITHUB_REPOSITORY openssl/openssl
    VERSION 1.0.2
    GIT_TAG OpenSSL_1_0_2r
    DOWNLOAD_ONLY TRUE
)

if(OpenSSL_ADDED)
    ExternalProject_Add(OpenSSL
        PREFIX openssl
        SOURCE_DIR ${OpenSSL_SOURCE_DIR}
        CONFIGURE_COMMAND ${OpenSSL_SOURCE_DIR}/Configure ${ARCH} no-shared no-dso no-engines -fPIC --prefix=<INSTALL_DIR>
        BUILD_IN_SOURCE 1
    )
    set(DEPENDS ${DEPENDS} OpenSSL)
    ExternalProject_Get_Property(OpenSSL INSTALL_DIR)
    set(LDFLAGS "${LDFLAGS} -L${INSTALL_DIR}/lib")
    set(CPPFLAGS "${CPPFLAGS} -I${INSTALL_DIR}/include")
    set(OPENSSL_CPPFLAGS "CPPFLAGS=-I${INSTALL_DIR}/include")
    set(OPENSSL_LDFLAGS "LDFLAGS=-L${INSTALL_DIR}/lib")
endif(OpenSSL_ADDED)

CPMAddPackage(
    NAME Libevent
    GITHUB_REPOSITORY libevent/libevent
    VERSION 2.1.8
    GIT_TAG release-2.1.8-stable
    DOWNLOAD_ONLY TRUE
)

if(Libevent_ADDED)
    ExternalProject_Add(Libevent
        PREFIX libevent
        DEPENDS ${DEPENDS}
        SOURCE_DIR ${Libevent_SOURCE_DIR}
        CONFIGURE_COMMAND ${Libevent_SOURCE_DIR}/autogen.sh
            && ${Libevent_SOURCE_DIR}/configure ${OPENSSL_CPPFLAGS} --enable-cxx --disable-shared --with-pic ${OPENSSL_LDFLAGS} --prefix=<INSTALL_DIR>
        BUILD_IN_SOURCE 1
    )
    set(DEPENDS ${DEPENDS} Libevent)
    ExternalProject_Get_Property(Libevent INSTALL_DIR)
    set(LDFLAGS "${LDFLAGS} -L${INSTALL_DIR}/lib")
    set(CPPFLAGS "${CPPFLAGS} -I${INSTALL_DIR}/include")
endif(Libevent_ADDED)

if(NOT ${DISABLE_WALLET})
    CPMAddPackage(
        NAME BerkeleyDB
        VERSION 4.8.30
        URL https://download.oracle.com/berkeley-db/db-4.8.30.NC.zip
        URL_HASH SHA256=43ecd76886992ea416fdadc54b7f2b83ef249d9a6964bd07708ccae42d0226ce
        DOWNLOAD_ONLY TRUE
    )

    if(NOT ${BerkeleyDB_VERSION} VERSION_LESS "5.0")
        set(OPTIONS "${OPTIONS} --with-incompatible-bdb")
    endif()

    if(BerkeleyDB_ADDED)
        ExternalProject_Add(BerkeleyDB
            PREFIX bdb
            SOURCE_DIR ${BerkeleyDB_SOURCE_DIR}
            PATCH_COMMAND sed -i "s/__atomic_compare_exchange/__atomic_compare_exchange_db/" ${BerkeleyDB_SOURCE_DIR}/dbinc/atomic.h
            CONFIGURE_COMMAND ${BerkeleyDB_SOURCE_DIR}/dist/configure --enable-cxx --disable-shared --with-pic --prefix=<INSTALL_DIR>
        )
        set(DEPENDS ${DEPENDS} BerkeleyDB)
        ExternalProject_Get_Property(BerkeleyDB INSTALL_DIR)
        set(LDFLAGS "${LDFLAGS} -L${INSTALL_DIR}/lib")
        set(CPPFLAGS "${CPPFLAGS} -I${INSTALL_DIR}/include")
    endif(BerkeleyDB_ADDED)
endif()

set(BOOST_LIBS chrono,filesystem,system,locale,thread)

string(REPLACE "," ";" BOOST_COMPONENTS ${BOOST_LIBS})

if(NOT ${DISABLE_TESTS})
    set(BOOST_LIBS ${BOOST_LIBS},test)
    set(BOOST_COMPONENTS ${BOOST_COMPONENTS};unit_test_framework)
endif()

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
        set(DEPENDS ${DEPENDS} ICU)
        ExternalProject_Get_Property(ICU INSTALL_DIR)
        set(ICU_PATH ${INSTALL_DIR})
        set(OPTIONS "${OPTIONS} --with-icu=${ICU_PATH}")
        set(LDFLAGS "${LDFLAGS} -L${ICU_PATH}/lib")
        set(CPPFLAGS "${CPPFLAGS} -I${ICU_PATH}/include")
    endif(ICU_ADDED)

    ExternalProject_Add(Boost
        PREFIX boost
        DEPENDS ${DEPENDS}
        SOURCE_DIR ${Boost_SOURCE_DIR}
        CONFIGURE_COMMAND ${Boost_SOURCE_DIR}/bootstrap.sh --with-icu=${ICU_PATH} --with-libraries=${BOOST_LIBS} && ${Boost_SOURCE_DIR}/b2 headers
        BUILD_COMMAND ${Boost_SOURCE_DIR}/b2 install threading=multi -sNO_BZIP2=1 -sNO_ZLIB=1 link=static linkflags="-L${ICU_PATH}/lib -licuio -licuuc -licudata -licui18n" cxxflags=-fPIC boost.locale.iconv=off boost.locale.posix=off boost.locale.icu=on boost.locale.std=off -sICU_PATH=${ICU_PATH} --prefix=<INSTALL_DIR>
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
    )
    set(DEPENDS ${DEPENDS} Boost)
    ExternalProject_Get_Property(Boost INSTALL_DIR)
    set(OPTIONS "${OPTIONS} --with-boost=${INSTALL_DIR}")
    set(LDFLAGS "${LDFLAGS} -L${INSTALL_DIR}/lib")
    set(CPPFLAGS "${CPPFLAGS} -I${INSTALL_DIR}/include")
    set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${Boost_SOURCE_DIR}/bin.v2)
endif(Boost_ADDED)

set(CPPFLAGS "${CPPFLAGS} -Wno-parentheses -Wno-unused-local-typedefs -Wno-deprecated -Wno-implicit-fallthrough -Wno-unused-parameter")

separate_arguments(OPTIONS)

ExternalProject_Add(lbrycrdd
    PREFIX lbrycrdd
    DEPENDS ${DEPENDS}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/autogen.sh
        && ${CMAKE_CURRENT_SOURCE_DIR}/configure ${OPTIONS} CPPFLAGS=${CPPFLAGS} LDFLAGS=${LDFLAGS} --prefix=<INSTALL_DIR>
    BUILD_IN_SOURCE 1
    BUILD_ALWAYS 1
)
