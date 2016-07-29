#! /bin/bash

set -euo pipefail

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
     g++-mingw-w64-i686 mingw-w64-i686-dev g++-mingw-w64-x86-64 \
     mingw-w64-x86-64-dev build-essential libtool autotools-dev automake pkg-config \
     libssl-dev libevent-dev bsdmainutils curl ca-certificates

cd depends
make HOST=i686-w64-mingw32 NO_QT=1
cd ..
patch -p1 < packaging/remove_consensus.patch
./autogen.sh
./configure --prefix=`pwd`/depends/i686-w64-mingw32 --without-gui
make

