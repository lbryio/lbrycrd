#!/bin/bash

set -euo pipefail

function HELP {
    echo "Build lbrycrd"
    echo "-----"
    echo "When run without any arguments, this script expects the current directory"
    echo "to be the lbrycrd repo and it builds what is in that directory"
    echo
    echo "This is a long build process so it can be split into two parts"
    echo "Specify the -d flag to build only the dependencies"
    echo "and the -l flag to build only lbrycrd. This will fail"
    echo "if the dependencies weren't built earlier"
    echo
    echo "Optional arguments:"
    echo
    echo "-c: clone a fresh copy of the repo"
    echo "-r: remove intermediate files."
    echo "-l: build only lbrycrd"
    echo "-d: build only the dependencies"
    echo "-h: show help"
    echo "-t: turn trace on"
    exit 1
}

CLONE=false
CLEAN=false
BUILD_DEPENDENCIES=true
BUILD_LBRYCRD=true

while getopts :crldth:w:d: FLAG; do
    case $FLAG in
	c)
	    CLONE=true
	    ;;
	r)
	    CLEAN=true
	    ;;
	l)
	    BUILD_DEPENDENCIES=false
	    ;;
	d)
	    BUILD_LBRYCRD=false
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

# two arguments
#  - pid (probably from $!)
#  - echo message
function wait_and_echo() {
    PID=$1
    (set -o | grep xtrace | grep -q on)
    TRACE_STATUS=$?
    # disable xtrace or else this will get verbose, which is what
    # I'm trying to avoid by going through all of this nonsense anyway
    set +o xtrace
    TIME=0
    SLEEP=5
    # loop until the process is no longer running
    # check every $SLEEP seconds, echoing a message every minute
    while (ps -p ${PID} > /dev/null); do
	sleep ${SLEEP}
	let "TIME += ${SLEEP}"
	if ! expr ${TIME} % 60 > /dev/null; then
	    echo $2
	fi
    done
    # restore the xtrace setting
    if [ ${TRACE_STATUS} -eq 0 ]; then
	set -o xtrace
    fi
}


function build_dependencies() {
    brew update > /dev/null
    brew install autoconf
    brew install automake
    brew install libtool
    brew install pkg-config
    brew install protobuf
    if ! brew ls | grep gmp --quiet; then
	brew install gmp
    fi

    if [ "$CLEAN" = true ]; then
	rm -rf "${LBRYCRD_DEPENDENCIES}"
    fi

    mkdir "${LBRYCRD_DEPENDENCIES}"
    mkdir "${LOG_DIR}"

    #download, patch, and build bdb
    cd "${LBRYCRD_DEPENDENCIES}"
    wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
    tar xf db-4.8.30.NC.tar.gz
    curl -OL https://raw.github.com/narkoleptik/os-x-berkeleydb-patch/master/atomic.patch
    patch db-4.8.30.NC/dbinc/atomic.h < atomic.patch
    cd db-4.8.30.NC/build_unix
    BDB_LOG="${LOG_DIR}/bdb_build.log"
    echo "Building bdb.  tail -f $BDB_LOG to see the details and monitor progress"
    ../dist/configure --prefix=$BDB_PREFIX --enable-cxx --disable-shared --with-pic > "${BDB_LOG}"
    make >> "${BDB_LOG}" 2>&1 &
    wait_and_echo $! "Waiting for bdb to finish building"
    make install >> "${BDB_LOG}" 2>&1


    #download and build openssl
    cd $LBRYCRD_DEPENDENCIES
    wget https://www.openssl.org/source/openssl-1.0.1p.tar.gz
    tar xf openssl-1.0.1p.tar.gz
    mkdir $OPENSSL_PREFIX
    mkdir $OPENSSL_PREFIX/ssl
    cd openssl-1.0.1p
    OPENSSL_LOG="${LOG_DIR}/openssl_build.log"
    echo "Building bdb.  tail -f $OPENSSL_LOG to see the details and monitor progress"
    ./Configure --prefix=$OPENSSL_PREFIX --openssldir=$OPENSSL_PREFIX/ssl -fPIC darwin64-x86_64-cc no-shared no-dso no-engines > "${OPENSSL_LOG}"
    make >> "${OPENSSL_LOG}" 2>&1 &
    wait_and_echo $! "Waiting for openssl to finish building"
    make install >> "${OPENSSL_LOG}" 2>&1

    set +u
    export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig/"
    set -u
    
    #download and build boost
    cd $LBRYCRD_DEPENDENCIES
    wget http://sourceforge.net/projects/boost/files/boost/1.59.0/boost_1_59_0.tar.bz2/download -O boost_1_59_0.tar.bz2
    tar xf boost_1_59_0.tar.bz2
    cd "${BOOST_ROOT}"
    BOOST_LOG="${LOG_DIR}/boost_build.log"
    echo "Building Boost.  tail -f ${BOOST_LOG} to see the details and monitor progress"
    ./bootstrap.sh > "${BOOST_LOG}"
    ./b2 link=static cxxflags=-fPIC stage >> "${BOOST_LOG}" 2>&1 &
    wait_and_echo $! "Waiting for boost to finish building"

    #download and build libevent
    cd $LBRYCRD_DEPENDENCIES
    mkdir libevent_build
    git clone https://github.com/libevent/libevent.git
    cd libevent
    LIBEVENT_LOG="${LOG_DIR}/libevent_build.log"
    echo "Building libevent.  tail -f ${LIBEVENT_LOG} to see the details and monitor progress"
    ./autogen.sh > "${LIBEVENT_LOG}"
    ./configure --prefix=$LIBEVENT_PREFIX --enable-static --disable-shared --with-pic LDFLAGS="-L${OPENSSL_PREFIX}/lib/" CPPFLAGS="-I${OPENSSL_PREFIX}/include" >> "${LIBEVENT_LOG}"
    make >> "${LIBEVENT_LOG}" 2>&1 &
    wait_and_echo $! "Waiting for libevent to finish building"
    make install >> "${LIBEVENT_LOG}"
}

function build_lbrycrd() {
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
}

# these variables are needed in both functions
LBRYCRD_DEPENDENCIES="`pwd`/dependencies"
LOG_DIR="${LBRYCRD_DEPENDENCIES}/log"
BDB_PREFIX="${LBRYCRD_DEPENDENCIES}/bdb"
OPENSSL_PREFIX="${LBRYCRD_DEPENDENCIES}/openssl_build"
export BOOST_ROOT="${LBRYCRD_DEPENDENCIES}/boost_1_59_0"
LIBEVENT_PREFIX="${LBRYCRD_DEPENDENCIES}/libevent_build"

if [ "${BUILD_DEPENDENCIES}" = true ]; then
    build_dependencies
fi

set +u
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig/:${LIBEVENT_PREFIX}/lib/pkgconfig/"
set -u

if [ "${BUILD_LBRYCRD}" = true ]; then
    build_lbrycrd
fi

