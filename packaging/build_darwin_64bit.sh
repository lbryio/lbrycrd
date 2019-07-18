#!/usr/bin/env bash

set -euo pipefail

# NOTE: this requires that you get the MacOS SDK separately. 
# To acquire it, you will need to log into the Apple dev portal. 
# From there, you download an Xcode package. Recommended: 7.3.1
# You can extract the SDK from that using contrib/macdeploy/extract
# you will need a folder like this: depends/SDKs/MacOSOSX10.11.sdk
# and ensure that the darwin.mk file version correspondes to the SDK.

if which dpkg-query >/dev/null; then
    if dpkg-query -W librsvg2-bin libtiff-tools cmake imagemagick libcap-dev libz-dev libbz2-dev python-setuptools \
            build-essential libtool autotools-dev automake pkg-config bsdmainutils curl ca-certificates; then
        echo "All dependencies satisfied."
    else
        echo "Missing dependencies detected. Exiting..."
        exit 1
    fi
fi

if [ ! -e depends/SDKs/MacOSX10.11.sdk ]; then
    echo "Missing depends/SDKs/MacOSX10.11.sdk"
    exit 1
fi

if which ccache >/dev/null; then
    echo "ccache config:"
    ccache -ps
fi

export CXXFLAGS="${CXXFLAGS:--O2 -frecord-gcc-switches}"
echo "CXXFLAGS set to $CXXFLAGS"

pushd depends
make -j`getconf _NPROCESSORS_ONLN` HOST=x86_64-apple-darwin14 NO_QT=1 V=1
popd

./autogen.sh
DEPS_DIR=`pwd`/depends/x86_64-apple-darwin14
CONFIG_SITE=${DEPS_DIR}/share/config.site ./configure --enable-reduce-exports --without-gui --with-icu="${DEPS_DIR}" --enable-static --disable-shared
make -j`getconf _NPROCESSORS_ONLN`
${DEPS_DIR}/native/bin/x86_64-apple-darwin14-strip src/lbrycrdd src/lbrycrd-cli src/lbrycrd-tx

if which ccache >/dev/null; then
    echo "ccache stats:"
    ccache -s
fi

echo "OSX 64bit build is complete"
