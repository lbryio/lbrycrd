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
tar -xvzf icu4c-57_1-src.tgz
pushd icu/source
./runConfigureICU Linux --prefix=$icu_linux_dir --enable-extras=no --enable-strict=no --enable-static --enable-shared=no --enable-tests=no --enable-samples=no --enable-dyload=no
make
make install
popd
popd

cd depends
mkdir -p sources
cp "$staging_dir/icu4c-57_1-src.tgz" sources/
make HOST=i686-w64-mingw32 NO_QT=1
cd ..
patch -p1 < packaging/remove_consensus.patch
./autogen.sh
icu_mingw_dir=$(cat /tmp/icu_install_dir)
CC="i686-w64-mingw32-gcc" ./configure --prefix=`pwd`/depends/i686-w64-mingw32 --host=i686-w64-mingw32 --build=i686-w64-mingw32 --without-gui --with-icu=$($icu_mingw_dir/bin/icu-config --prefix) --enable-static --disable-shared
make

rm -rf $staging_dir
