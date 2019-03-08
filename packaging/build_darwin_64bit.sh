#!/usr/bin/env bash

set -euo pipefail

# NOTE: this requires that you get the MacOS SDK separately. 
# To acquire it, you will need to log into the Apple dev portal. 
# From there, you download an Xcode package. Recommended: 7.3.1
# You can extract the SDK from that using contrib/macdeploy/extract
# you will need a folder like this: depends/SDKs/MacOSOSX10.11.sdk
# and ensure that the darwin.mk file version correspondes to the SDK.

if [[ "${1-unset}" == "update" ]]; then
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends \
     librsvg2-bin libtiff-tools cmake imagemagick libcap-dev libz-dev libbz2-dev python-setuptools \
     build-essential libtool autotools-dev automake pkg-config ccache \
     bsdmainutils curl ca-certificates
  sudo update-alternatives --config x86_64-apple-darwin14-g++ # you have to select posix
fi

echo "ccache config:"
ccache -ps

pushd depends
make -j`nproc` HOST=x86_64-apple-darwin14 NO_QT=1 V=1
popd

./autogen.sh
DEPS_DIR=`pwd`/depends/x86_64-apple-darwin14
CONFIG_SITE=${DEPS_DIR}/share/config.site ./configure --enable-reduce-exports --without-gui --with-icu="${DEPS_DIR}" --enable-static --disable-shared
make -j`nproc`
${DEPS_DIR}/native/bin/x86_64-apple-darwin14-strip src/lbrycrdd src/lbrycrd-cli src/lbrycrd-tx

echo "ccache stats:"
ccache -s

echo "OSX 64bit build is complete"
