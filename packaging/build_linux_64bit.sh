#!/usr/bin/env bash

set -euo pipefail

if which dpkg-query >/dev/null; then
    if dpkg-query -W libtool autotools-dev automake pkg-config bsdmainutils curl ca-certificates; then
        echo "All dependencies satisfied."
    else
        echo "Missing dependencies detected. Exiting..."
        exit 1
    fi
fi

if which ccache >/dev/null; then
    echo "ccache config:"
    ccache -ps
fi

export CXXFLAGS="${CXXFLAGS:--frecord-gcc-switches}"
echo "CXXFLAGS set to $CXXFLAGS"

cd depends
make -j`getconf _NPROCESSORS_ONLN` HOST=x86_64-pc-linux-gnu NO_QT=1 V=1
cd ..

./autogen.sh
DEPS_DIR=`pwd`/depends/x86_64-pc-linux-gnu
CONFIG_SITE=${DEPS_DIR}/share/config.site ./configure --enable-static --disable-shared --with-pic --without-gui
make -j`getconf _NPROCESSORS_ONLN`
strip src/lbrycrdd src/lbrycrd-cli src/lbrycrd-tx

if which ccache >/dev/null; then
    echo "ccache stats:"
    ccache -s
fi

echo "Linux 64bit build is complete"