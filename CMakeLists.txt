cmake_minimum_required(VERSION 3.7)
project(lbrycrd_clion) # Do not use for full compile. This is for CLion syntax checking only. 

set (CMAKE_CXX_STANDARD 11)

if(EXISTS "build/boost")
    set(BOOST_ROOT "build/boost" CACHE PATH "Boost library path")
    set(Boost_NO_SYSTEM_PATHS on CACHE BOOL "Do not search system for Boost")
endif()
find_package(Boost REQUIRED COMPONENTS filesystem thread chrono locale)

file(GLOB sources
        src/*.h src/*.cpp
        src/wallet/*.h src/wallet/*.cpp
        src/support/*.h src/support/*.cpp src/support/allocators/*.h
        src/script/*.h src/script/*.cpp
        src/index/*.h src/index/*.cpp
        src/interfaces/*.h src/interfaces/*.cpp
        src/primitives/*.h src/primitives/*.cpp
        src/policy/*.h src/policy/*.cpp
        src/crypto/*.h src/crypto/*.cpp
        src/consensus/*.h src/consensus/*.cpp
        src/compat/*.h src/compat/*.cpp
        src/rpc/*.h src/rpc/*.cpp
    )
list(FILTER sources EXCLUDE REGEX "src/bitcoin*.cpp$")

include_directories(${Boost_INCLUDE_DIRS}
        build/bdb/include
        build/libevent/include
        build/openssl/include
        src/support/allocators
        src/support
        src/rpc
        src/policy
        src/wallet src/script
        src/leveldb/helpers/memenv
        src/leveldb/include
        src/config
        src/crypto
        src/compat
        src/obj
        src/univalue/include
        src/secp256k1/include
        src/
    )

add_compile_definitions(HAVE_CONFIG_H)

add_executable(lbrycrd-cli src/bitcoin-cli.cpp ${sources})
add_executable(lbrycrd-tx src/bitcoin-tx.cpp ${sources})
add_executable(lbrycrdd src/bitcoind.cpp ${sources})

file(GLOB tests src/test/*.cpp)
add_executable(test_lbrycrd ${tests} ${sources})
target_include_directories(test_lbrycrd PRIVATE src/test)