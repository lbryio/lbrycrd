#!/bin/bash

set -euo pipefail

function HELP {
    echo "Build lbrycrd"
    echo "-----"
    echo "When run without any arguments, this script expects the current directory"
    echo "to be the lbrycrd repo and it builds what is in that directory"
    echo
    echo "Optional arguments:"
    echo
    echo "-c: clone a fresh copy of the repo"
    echo "-h: show help"
    echo "-t: turn trace on"
    exit 1
}

CLONE=false


while getopts :hctb:w:d: FLAG; do
    case $FLAG in
	c)
	    CLONE=true
	    ;;
	t)
	    set -o xtrace
	    ;;
	h)
	    HELP
	    ;;
	\?) #unrecognized option - show help
	    echo "Option -$OPTARG not allowed."
	    HELP
	    ;;
	:)
	    echo "Option -$OPTARG requires an argument."
	    HELP
	    ;;
    esac
done

shift $((OPTIND-1))


if [ "$CLONE" = false ]; then
    if [ `basename $PWD` != "lbrycrd" ]; then
	echo "Not currently in the lbrycrd directory. Cowardly refusing to go forward"
	exit 1
    fi
    SOURCE_DIR=$PWD
fi

brew update
brew install autoconf
brew install automake
brew install libtool
brew install pkg-config
brew install protobuf
if ! brew ls | grep gmp --quiet; then
    brew install gmp
fi

mkdir dependencies
cd dependencies
export LBRYCRD_DEPENDENCIES="`pwd`"

#download, patch, and build bdb
wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
tar xf db-4.8.30.NC.tar.gz
export BDB_PREFIX="`pwd`/bdb"
curl -OL https://raw.github.com/narkoleptik/os-x-berkeleydb-patch/master/atomic.patch
patch db-4.8.30.NC/dbinc/atomic.h < atomic.patch
cd db-4.8.30.NC/build_unix
../dist/configure --prefix=$BDB_PREFIX --enable-cxx --disable-shared --with-pic
make
make install

#download and build openssl
cd $LBRYCRD_DEPENDENCIES
wget https://www.openssl.org/source/openssl-1.0.1p.tar.gz
tar xf openssl-1.0.1p.tar.gz
export OPENSSL_PREFIX="`pwd`/openssl_build"
mkdir $OPENSSL_PREFIX
mkdir $OPENSSL_PREFIX/ssl
cd openssl-1.0.1p
./Configure --prefix=$OPENSSL_PREFIX --openssldir=$OPENSSL_PREFIX/ssl -fPIC darwin64-x86_64-cc no-shared no-dso no-engines
make
make install

#download and build boost
cd $LBRYCRD_DEPENDENCIES
wget http://sourceforge.net/projects/boost/files/boost/1.59.0/boost_1_59_0.tar.bz2/download -O boost_1_59_0.tar.bz2
tar xf boost_1_59_0.tar.bz2
export BOOST_ROOT="`pwd`/boost_1_59_0"
cd boost_1_59_0
./bootstrap.sh
./b2 link=static cxxflags=-fPIC stage

#download and build libevent
cd $LBRYCRD_DEPENDENCIES
mkdir libevent_build
git clone https://github.com/libevent/libevent.git
export LIBEVENT_PREFIX="`pwd`/libevent_build"
cd libevent
./autogen.sh
./configure --prefix=$LIBEVENT_PREFIX --enable-static --disable-shared --with-pic LDFLAGS="-L${OPENSSL_PREFIX}/lib/" CPPFLAGS="-I${OPENSSL_PREFIX}/include"
make
make install
set +u
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig/:${LIBEVENT_PREFIX}/lib/pkgconfig/"
set -u

#download and build lbrycrd
if [ "$CLONE" == true ]; then
    cd $LBRYCRD_DEPENDENCIES
    git clone https://github.com/lbryio/lbrycrd
    cd lbrycrd
else
    cd "${SOURCE_DIR}"
fi
./autogen.sh
./configure --without-gui --enable-cxx --enable-static --disable-shared --with-pic \
    LDFLAGS="-L${OPENSSL_PREFIX}/lib/ -L${BDB_PREFIX}/lib/ -L${LIBEVENT_PREFIX}/lib/ -static-libstdc++" \
    CPPFLAGS="-I${OPENSSL_PREFIX}/include -I${BDB_PREFIX}/include -I${LIBEVENT_PREFIX}/include/"

make
strip src/lbrycrdd
strip src/lbrycrd-cli
strip src/lbrycrd-tx
strip src/test/test_lbrycrd
