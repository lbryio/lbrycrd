#!/usr/bin/env bash

set -euo pipefail

cd depends
make -j`nproc` HOST=x86_64-pc-linux-gnu NO_QT=1
cd ..

./autogen.sh
./configure -prefix="$PWD/depends/x86_64-pc-linux-gnu" --enable-static --disable-shared --with-pic --without-gui CXXFLAGS="-fno-omit-frame-pointer"
make -j`nproc`
