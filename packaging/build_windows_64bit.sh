#!/usr/bin/env bash

set -euo pipefail

if [[ "${1-unset}" == "update" ]]; then
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends \
     g++-mingw-w64-x86-64 mingw-w64-x86-64-dev \
     build-essential libtool autotools-dev automake pkg-config \
     libssl-dev libevent-dev bsdmainutils curl ca-certificates
  sudo update-alternatives --config x86_64-w64-mingw32-g++ # you have to select posix
fi

pushd depends
make -j`nproc` HOST=x86_64-w64-mingw32 NO_QT=1 V=1
popd

./autogen.sh
DEPS_DIR=`pwd`/depends/x86_64-w64-mingw32
CONFIG_SITE=${DEPS_DIR}/share/config.site ./configure --prefix=/ --without-gui --with-icu="$DEPS_DIR" --enable-static --disable-shared
make -j`nproc`
x86_64-w64-mingw32-strip src/lbrycrdd.exe src/lbrycrd-cli.exe src/lbrycrd-tx.exe

echo "Windows 64bit build is complete"
