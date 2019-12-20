cmake_minimum_required(VERSION 3.10)

project(libclaimtrie)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(../../contrib/cmake/cmake/CPM.cmake)
include(ExternalProject)

set(CLAIMTRIE_SRC
    blob.cpp
    data.cpp
    forks.cpp
    hashes.cpp
    log.cpp
    trie.cpp
    txoutpoint.cpp
    uints.cpp
    ./sqlite/sqlite3.c
)

if(BIND)
    find_program(SWIG NAMES swig)
    string(TOLOWER ${BIND} BIND)
    if(${BIND} STREQUAL "python")
        find_package(PythonInterp 3.6 REQUIRED)
        find_package(PythonLibs 3.6 REQUIRED)
        set(BIND_INCLUDE_DIRS ${PYTHON_INCLUDE_DIRS})
        set(SWIG_OPTIONS -python;-py3)
        set(CMAKE_SHARED_LIBRARY_PREFIX _lib)
    elseif(${BIND} STREQUAL "go")
        find_program(GOLANG NAMES go)
        if(NOT GOLANG)
            message(FATAL_ERROR "Golang was not found in system path")
        endif()
        execute_process(COMMAND ${GOLANG} "version" OUTPUT_VARIABLE GOLANG_VERSION)
        STRING(REGEX MATCH "[0-9]+.[0-9]+.[0-9]" GOLANG_VERSION "${GOLANG_VERSION}")
        if(GOLANG_VERSION VERSION_LESS 1.5)
            message(FATAL_ERROR "Update Golang to at least 1.5, now " ${GOLANG_VERSION})
        endif()
        set(SWIG_OPTIONS -go;-cgo;-intgosize;32)
        set(POST_BUILD_COMMAND go)
        set(POST_BUILD_ARGS install;-x;${CMAKE_PROJECT_NAME}.go)
        # CGO_LDFLAGS is buggy so we should inject linker flags in generated go file
        set(POST_BUILD_PATCH -i "\"s/import \\\"C\\\"/\\/\\/ #cgo LDFLAGS: -L\\\$$\\{SRCDIR\\} -lclaimtrie\\nimport \\\"C\\\"/\"" ${CMAKE_PROJECT_NAME}.go)
    else()
        message(FATAL_ERROR "Implement a handler for ${BIND}")
    endif()
    add_custom_command(OUTPUT ${CMAKE_PROJECT_NAME}_wrap.cxx
        COMMAND ${SWIG}
        ARGS -c++ ${SWIG_OPTIONS} -outcurrentdir ${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}.i
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    set(CLAIMTRIE_SRC ${CLAIMTRIE_SRC}
        ${CMAKE_PROJECT_NAME}_wrap.cxx
    )
endif()

add_library(claimtrie SHARED ${CLAIMTRIE_SRC})
if (POST_BUILD_PATCH)
    add_custom_command(TARGET claimtrie POST_BUILD
        COMMAND sed
        ARGS ${POST_BUILD_PATCH}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
if(POST_BUILD_COMMAND)
    add_custom_command(TARGET claimtrie POST_BUILD
        COMMAND ${POST_BUILD_COMMAND}
        ARGS ${POST_BUILD_ARGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

target_include_directories(claimtrie PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(BIND_INCLUDE_DIRS)
    target_include_directories(claimtrie PRIVATE ${BIND_INCLUDE_DIRS})
endif()

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

set(BOOST_COMPONENTS filesystem;locale;system;chrono;thread;unit_test_framework)

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
