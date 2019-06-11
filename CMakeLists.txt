
cmake_minimum_required(VERSION 3.10)

project(lbrycrd)
set(CMAKE_CXX_STANDARD 11)

include(cmake/CPM.cmake)
include(ExternalProject)

set(OPTIONS "" CACHE STRING "lbrycrdd configure options")
set(CPPFLAGS "" CACHE STRING "lbrycrdd compiler options")
set(LDFLAGS "" CACHE STRING "lbrycrdd linker options")

if(NOT ${CPM_USE_LOCAL_PACKAGES})
    set(OPTIONS "${OPTIONS} --enable-static --disable-shared")
else()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
endif()

set(OPTIONS "--enable-cxx --without-gui ${OPTIONS} --with-pic")

string(FIND ${OPTIONS} "--disable-tests" TESTS_DISABLED)
string(FIND ${OPTIONS} "--disable-wallet" WALLET_DISABLED)

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
        CONFIGURE_COMMAND ${OpenSSL_SOURCE_DIR}/Configure linux-x86_64 no-shared no-dso no-engines -fPIC --prefix=<INSTALL_DIR>
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

if(NOT ${WALLET_DISABLED} GREATER -1)
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
            PATCH_COMMAND patch -d ${BerkeleyDB_SOURCE_DIR}/dbinc -i ${CMAKE_CURRENT_SOURCE_DIR}/contrib/patches/atomic.patch -f
            CONFIGURE_COMMAND ${BerkeleyDB_SOURCE_DIR}/dist/configure --enable-cxx --disable-shared --with-pic --prefix=<INSTALL_DIR>
        )
        set(DEPENDS ${DEPENDS} BerkeleyDB)
        ExternalProject_Get_Property(BerkeleyDB INSTALL_DIR)
        set(LDFLAGS "${LDFLAGS} -L${INSTALL_DIR}/lib")
        set(CPPFLAGS "${CPPFLAGS} -I${INSTALL_DIR}/include")
    endif(BerkeleyDB_ADDED)
endif()

CPMAddPackage(
    NAME ICU
    GITHUB_REPOSITORY unicode-org/icu
    VERSION 63.1
    GIT_TAG release-63-1
    DOWNLOAD_ONLY TRUE
)

if(ICU_ADDED)
    ExternalProject_Add(ICU
        PREFIX icu
        SOURCE_DIR ${ICU_SOURCE_DIR}
        CONFIGURE_COMMAND ${ICU_SOURCE_DIR}/icu4c/source/configure --enable-draft --enable-tools
                    --disable-shared --enable-static --disable-extras --disable-icuio --disable-dyload
                    --disable-layout --disable-layoutex --disable-tests --disable-samples CFLAGS=-fPIC CPPFLAGS=-fPIC --prefix=<INSTALL_DIR>
    )
    set(DEPENDS ${DEPENDS} ICU)
    ExternalProject_Get_Property(ICU INSTALL_DIR)
    set(ICU_PATH ${INSTALL_DIR})
    set(OPTIONS "${OPTIONS} --with-icu=${ICU_PATH}")
    set(LDFLAGS "${LDFLAGS} -L${ICU_PATH}/lib")
    set(CPPFLAGS "${CPPFLAGS} -I${ICU_PATH}/include")
endif(ICU_ADDED)

set(BOOST_LIBS chrono,filesystem,program_options,system,locale,regex,thread)

if(NOT ${TESTS_DISABLED} GREATER -1)
    set(BOOST_LIBS ${BOOST_LIBS},unit_test_framework)
endif()

string(REPLACE "," ";" BOOST_COMPONENTS ${BOOST_LIBS})

CPMAddPackage(
    NAME Boost
    GITHUB_REPOSITORY boostorg/boost
    VERSION 1.64.0
    COMPONENTS ${BOOST_COMPONENTS}
    GIT_TAG boost-1.64.0
    GIT_SUBMODULES libs/* tools/*
    DOWNLOAD_ONLY TRUE
)

if(Boost_ADDED)
    ExternalProject_Add(Boost
        PREFIX boost
        DEPENDS ${DEPENDS}
        SOURCE_DIR ${Boost_SOURCE_DIR}
        CONFIGURE_COMMAND ${Boost_SOURCE_DIR}/bootstrap.sh --with-icu=${ICU_PATH} --with-libraries=${BOOST_LIBS} && ${Boost_SOURCE_DIR}/b2 headers
        BUILD_COMMAND ${Boost_SOURCE_DIR}/b2 install link=static cxxflags=-fPIC boost.locale.iconv=off boost.locale.posix=off -sICU_PATH=${ICU_PATH} --prefix=<INSTALL_DIR>
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
    )
    set(DEPENDS ${DEPENDS} Boost)
    ExternalProject_Get_Property(Boost INSTALL_DIR)
    set(OPTIONS "${OPTIONS} --with-boost=${INSTALL_DIR}")
    set(LDFLAGS "${LDFLAGS} -L${INSTALL_DIR}/lib")
    set(CPPFLAGS "${CPPFLAGS} -I${INSTALL_DIR}/include")
endif(Boost_ADDED)

set(${CPPFLAGS} "${CPPFLAGS} -Wno-unused-local-typedefs -Wno-deprecated -Wno-implicit-fallthrough -Wno-unused-parameter")

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
