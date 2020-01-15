OSX_MIN_VERSION=10.10
OSX_SDK_VERSION=10.11
OSX_SDK=$(SDK_PATH)/MacOSX$(OSX_SDK_VERSION).sdk
LD64_VERSION=253.9

darwin_CC=clang -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) -isysroot $(OSX_SDK) -mlinker-version=$(LD64_VERSION) -B $(host_prefix)/native/bin
darwin_CXX=clang++ -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) -isysroot $(OSX_SDK) -mlinker-version=$(LD64_VERSION) -stdlib=libc++ -B $(host_prefix)/native/bin

darwin_CFLAGS=-pipe
darwin_CXXFLAGS=$(darwin_CFLAGS) -std=c++11

darwin_release_CFLAGS=-O2
darwin_release_CXXFLAGS=$(darwin_release_CFLAGS)

darwin_debug_CFLAGS=-Og
darwin_debug_CXXFLAGS=$(darwin_debug_CFLAGS)

darwin_native_toolchain=native_cctools