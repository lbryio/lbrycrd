brew update
brew install autoconf
brew install automake
brew install libtool
brew install pkg-config
brew install protobuf
brew install gmp

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
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig/"

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
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${LIBEVENT_PREFIX}/lib/pkgconfig/"

#download and build lbrycrd
cd $LBRYCRD_DEPENDENCIES
git clone https://github.com/lbryio/lbrycrd
cd lbrycrd
git checkout real2
./autogen.sh
./configure --without-gui --enable-cxx --enable-static --disable-shared --with-pic \
            LDFLAGS="-L${OPENSSL_PREFIX}/lib/ -L${BDB_PREFIX}/lib/ -L${LIBEVENT_PREFIX}/lib/ -static-libstdc++" \
            CPPFLAGS="-I${OPENSSL_PREFIX}/include -I${BDB_PREFIX}/include -I${LIBEVENT_PREFIX}/include/"

make
strip src/lbrycrdd
strip src/lbrycrd-cli
strip src/lbrycrd-tx
strip src/test/test-lbrycrd