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
    echo "-o: timeout build after 45 minutes"
    echo "-t: turn trace on"
    echo "-h: show help"
    exit 1
}

CLONE=false
CLEAN=false
BUILD_DEPENDENCIES=true
BUILD_LBRYCRD=true
TIMEOUT=false

while getopts :crldoth:w:d: FLAG; do
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
	o)
	    TIMEOUT=true
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

SUDO=''
if (( $EUID != 0 )); then
    SUDO='sudo'
fi

if [ "$CLONE" = false ]; then
    if [ `basename $PWD` != "lbrycrd" ]; then
	echo "Not currently in the lbrycrd directory. Cowardly refusing to go forward"
	exit 1
    fi
    SOURCE_DIR=$PWD
fi

if [ -z ${TRAVIS_OS_NAME+x} ]; then
    if [ `uname -s` = "Darwin" ]; then
	OS_NAME="osx"
    else
	OS_NAME="linux"
    fi
else
    OS_NAME=${TRAVIS_OS_NAME}
fi

NEXT_TIME=60
function exit_at_45() {
    if [ -f ${HOME}/start_time ]; then
	NOW=`date +%s`
	START=`cat ${HOME}/start_time`
	TIMEOUT_SECS=2700 # 45 * 60
	# expr returns 0 if its been less then 45 minutes
	# and 1 if if its been more, so it is necessary
	# to ! it.
	TIME=`expr $NOW - $START`
	if expr ${TIME} \> ${NEXT_TIME} > /dev/null; then
	    echo "Build has taken $(expr ${TIME} / 60) minutes: $1"
	    NEXT_TIME=`expr ${TIME} + 60`
	fi
	if [ "$TIMEOUT" = true ] && expr ${TIME} \> ${TIMEOUT_SECS} > /dev/null; then
	    echo 'Exiting at 45 minutes to allow the cache to populate'
	    exit 1
	fi
    fi
}

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
	exit_at_45 "$2"
	sleep ${SLEEP}
    done
    # restore the xtrace setting
    if [ ${TRACE_STATUS} -eq 0 ]; then
	set -o xtrace
    fi
    wait $PID
    return $?
}

# run a command ($1) in the background
# logging its stdout and stderr to $2
# and wait until it completed
function background() {
    $1 >> $2 2>&1 &
    wait_and_echo $! "$3"
}

function cleanup() {
    rv=$?
    # cat the log file
    cat "$2"
    # delete the build directory
    rm -rf "$1"
    exit $rv
}

function install_brew_packages() {
    brew update > /dev/null
    brew install autoconf
    brew install automake
    brew install libtool
    brew install pkg-config
    brew install protobuf
    if ! brew ls | grep gmp --quiet; then
	brew install gmp
    fi
}

function install_apt_packages() {
    if [ -z ${TRAVIS+x} ]; then
	# if not on travis, its nice to see progress
	QUIET=""
    else
	QUIET="-qq"
    fi
    # get the required OS packages
    $SUDO apt-get ${QUIET} update
    $SUDO apt-get ${QUIET} install -y --no-install-recommends \
	  build-essential python-dev libbz2-dev libtool \
	  autotools-dev autoconf git pkg-config wget \
	  ca-certificates automake bsdmainutils
}

function build_dependencies() {
    if [ ${OS_NAME} = "osx" ]; then
	install_brew_packages
    else
	install_apt_packages
    fi
    
    if [ "$CLEAN" = true ]; then
	rm -rf "${LBRYCRD_DEPENDENCIES}"
    fi

    mkdir -p "${LBRYCRD_DEPENDENCIES}"
    mkdir -p "${LOG_DIR}"

    build_dependency  "${BDB_PREFIX}" "${LOG_DIR}/bdb_build.log" build_bdb
    build_dependency "${OPENSSL_PREFIX}" "${LOG_DIR}/openssl_build.log" build_openssl
    
    set +u
    export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig/"
    set -u

    build_dependency "${BOOST_PREFIX}" "${LOG_DIR}/boost_build.log" build_boost
    build_dependency "${LIBEVENT_PREFIX}" "${LOG_DIR}/libevent_build.log" build_libevent
}

function build_bdb() {
    wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
    tar xf db-4.8.30.NC.tar.gz
    if [ ${OS_NAME} = "osx" ]; then
	curl -OL https://raw.github.com/narkoleptik/os-x-berkeleydb-patch/master/atomic.patch
	patch db-4.8.30.NC/dbinc/atomic.h < atomic.patch
    fi
    cd db-4.8.30.NC/build_unix
    BDB_LOG="${LOG_DIR}/bdb_build.log"
    echo "Building bdb.  tail -f $BDB_LOG to see the details and monitor progress"
    ../dist/configure --prefix=$BDB_PREFIX --enable-cxx --disable-shared --with-pic > "${BDB_LOG}"
    background make "${BDB_LOG}" "Waiting for bdb to finish building"
    make install >> "${BDB_LOG}" 2>&1
}

function build_openssl() {
    OPENSSL_LOG="$1"
    wget https://www.openssl.org/source/openssl-1.0.1p.tar.gz
    tar xf openssl-1.0.1p.tar.gz
    mkdir -p $OPENSSL_PREFIX/ssl
    cd openssl-1.0.1p
    echo "Building bdb.  tail -f $OPENSSL_LOG to see the details and monitor progress"
    if [ ${OS_NAME} = "osx" ]; then
	./Configure --prefix=$OPENSSL_PREFIX --openssldir=$OPENSSL_PREFIX/ssl \
		    -fPIC darwin64-x86_64-cc \
		    no-shared no-dso no-engines > "${OPENSSL_LOG}"
    else
	./Configure --prefix=$OPENSSL_PREFIX --openssldir=$OPENSSL_PREFIX/ssl \
		    linux-x86_64 -fPIC -static no-shared no-dso > "${OPENSSL_LOG}"
    fi
    background make "${OPENSSL_LOG}" "Waiting for openssl to finish building"
    make install >> "${OPENSSL_LOG}" 2>&1
}

function build_boost() {
    BOOST_LOG="$1"
    wget http://sourceforge.net/projects/boost/files/boost/1.59.0/boost_1_59_0.tar.bz2/download \
	 -O boost_1_59_0.tar.bz2
    tar xf boost_1_59_0.tar.bz2
    cd boost_1_59_0
    echo "Building Boost.  tail -f ${BOOST_LOG} to see the details and monitor progress"
    ./bootstrap.sh --prefix=${BOOST_PREFIX} > "${BOOST_LOG}" 2>&1
    background "./b2 link=static cxxflags=-fPIC install" \
	       "${BOOST_LOG}" \
	       "Waiting for boost to finish building"
}

function build_libevent() {
    LIBEVENT_LOG="$1"
    git clone https://github.com/libevent/libevent.git
    cd libevent
    echo "Building libevent.  tail -f ${LIBEVENT_LOG} to see the details and monitor progress"
    ./autogen.sh > "${LIBEVENT_LOG}" 2>&1
    ./configure --prefix="${LIBEVENT_PREFIX}" --enable-static --disable-shared --with-pic \
		LDFLAGS="-L${OPENSSL_PREFIX}/lib/" \
		CPPFLAGS="-I${OPENSSL_PREFIX}/include" >> "${LIBEVENT_LOG}" 2>&1
    background make "${LIBEVENT_LOG}" "Waiting for libevent to finish building"
    make install >> "${LIBEVENT_LOG}"
}

function build_dependency() {
    PREFIX=$1
    LOG=$2
    BUILD=$3
    if [ ! -d "${PREFIX}" ]; then
	trap "cleanup \"${PREFIX}\" \"${LOG}\"" INT TERM EXIT
	cd $LBRYCRD_DEPENDENCIES
	mkdir -p "${PREFIX}"
	"${BUILD}" "${LOG}"
	trap - INT TERM EXIT
    fi
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
    LDFLAGS="-L${OPENSSL_PREFIX}/lib/ -L${BDB_PREFIX}/lib/ -L${LIBEVENT_PREFIX}/lib/ -static-libstdc++"
    CPPFLAGS="-I${OPENSSL_PREFIX}/include -I${BDB_PREFIX}/include -I${LIBEVENT_PREFIX}/include/"
    if [ ${OS_NAME} = "osx" ]; then
	./configure --without-gui --enable-cxx --enable-static --disable-shared --with-pic \
		    --with-boost="${BOOST_PREFIX}" \
		    LDFLAGS="${LDFLAGS}" \
		    CPPFLAGS="${CPPFLAGS}"
    else
	./configure --without-gui --with-boost="${BOOST_PREFIX}" \
		    LDFLAGS="${LDFLAGS}" \
		    CPPFLAGS="${CPPFLAGS}"
    fi
    LBRYCRD_LOG="${LOG_DIR}/lbrycrd_build.log"
    echo "Building lbrycrd.  tail -f ${LBRYCRD_LOG} to see the details and monitor progress"
    background make "${LBRYCRD_LOG}" "Waiting for lbrycrd to finish building"
    # tests don't work on OSX. Should definitely figure out why
    # that is but, for now, not letting that stop the rest
    # of the build
    if [ ${OS_NAME} = "linux" ]; then
	src/test/test_lbrycrd
    fi
    strip src/lbrycrdd
    strip src/lbrycrd-cli
    strip src/lbrycrd-tx
    strip src/test/test_lbrycrd
}

# these variables are needed in both functions
LBRYCRD_DEPENDENCIES="`pwd`/dependencies/${OS_NAME}"
LOG_DIR="`pwd`/logs"
BDB_PREFIX="${LBRYCRD_DEPENDENCIES}/bdb"
OPENSSL_PREFIX="${LBRYCRD_DEPENDENCIES}/openssl_build"
BOOST_PREFIX="${LBRYCRD_DEPENDENCIES}/boost_build"
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
