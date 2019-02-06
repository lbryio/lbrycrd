#! /bin/bash

set -euo pipefail

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
     g++-mingw-w64-i686 mingw-w64-i686-dev g++-mingw-w64-x86-64 \
     mingw-w64-x86-64-dev build-essential libtool autotools-dev automake pkg-config \
     libssl-dev libevent-dev bsdmainutils curl ca-certificates

#################################################################
# Build ICU for Linux first so that we can cross compile it below
# It's a strange ICU thing in that it requries a working 
# Linux build of itself to be used as part of the cross-compile
#################################################################
staging_dir=/tmp/icu_staging
icu_linux_dir=$staging_dir/build_icu_linux
mkdir -p $staging_dir
pushd $staging_dir
wget -c http://download.icu-project.org/files/icu4c/57.1/icu4c-57_1-src.tgz
tar -xzf icu4c-57_1-src.tgz
pushd icu/source
CC="gcc" CXX="g++" ./runConfigureICU Linux --prefix=$icu_linux_dir --enable-extras=no --enable-strict=no --enable-static --enable-shared=no --enable-tests=no --enable-samples=no --enable-dyload=no
make -j4
make install
popd
popd

pushd depends
# Remove the dir saying that dependencies are built (although ccache
# is still enabled).
rm -rf built
mkdir -p sources
cp "$staging_dir/icu4c-57_1-src.tgz" sources/

# Build and install the cross compiled ICU package.
make -j4 HOST=i686-w64-mingw32 NO_QT=1 ICU_ONLY=1

# Then build the rest of the dependencies (now that it exists and we
# can determine the location for it).
icu_mingw_dir=$(find /tmp/icu_install -name i686-w64-mingw32 -type d)
make -j4 HOST=i686-w64-mingw32 NO_QT=1 ICU_DIR=$icu_mingw_dir V=1
popd

./autogen.sh
echo "Using --with-icu=$icu_mingw_dir"
PREFIX=`pwd`/depends/i686-w64-mingw32
CC="i686-w64-mingw32-gcc" CXX="i686-w64-mingw32-g++" ./configure --prefix=$PREFIX --host=i686-w64-mingw32 --build=i686-w64-mingw32 --without-gui --with-icu=$icu_mingw_dir --enable-static --disable-shared
make -j4

rm -rf $staging_dir
# Remove hardcoded cross compiled ICU package path.
rm -rf /tmp/icu_install
echo "Windows build is complete"
