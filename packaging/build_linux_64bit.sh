#!/usr/bin/env bash

set -euo pipefail

cd depends
make -j`nproc` HOST=x86_64-pc-linux-gnu NO_QT=1
cd ..

./autogen.sh
DEPS_DIR=`pwd`/depends/x86_64-pc-linux-gnu
CONFIG_SITE=${DEPS_DIR}/share/config.site ./configure --enable-static --disable-shared --with-pic --without-gui CXXFLAGS="-fno-omit-frame-pointer"
make -j`nproc`
strip src/lbrycrdd src/lbrycrd-cli src/lbrycrd-tx

echo "Linux 64bit build is complete"