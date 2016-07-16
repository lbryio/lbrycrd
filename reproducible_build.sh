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
    echo "-c: don't clone a fresh copy of the repo"
    echo "-r: remove intermediate files."
    echo "-l: build only lbrycrd"
    echo "-d: build only the dependencies"
    echo "-o: timeout build after 45 minutes"
    echo "-t: turn trace on"
    echo "-h: show help"
    exit 1
}

CLONE=true
CLEAN=false
BUILD_DEPENDENCIES=true
BUILD_LBRYCRD=true
TIMEOUT=false
THREE_MB=3145728

while getopts :crldoth:w:d: FLAG; do
    case $FLAG in
	c)
	    CLONE=false
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
if (( EUID != 0 )); then
    SUDO='sudo'
fi

if [ "${CLONE}" = false ]; then
    if [ "$(basename "$PWD")" != "lbrycrd" ]; then
	echo "Not currently in the lbrycrd directory. Cowardly refusing to go forward"
	exit 1
    fi
    SOURCE_DIR=$PWD
fi

if [ -z "${TRAVIS_OS_NAME+x}" ]; then
    if [ "$(uname -s)" = "Darwin" ]; then
	OS_NAME="osx"
    else
	OS_NAME="linux"
    fi
else
    OS_NAME="${TRAVIS_OS_NAME}"
fi

if [ -z "${TRAVIS_BUILD_DIR+x}" ]; then
    START_TIME_FILE="$PWD/start_time"
else
    # if we are on travis (the primary use case for setting a timeout)
    # this file is created when the build starts
    START_TIME_FILE="$TRAVIS_BUILD_DIR/start_time"
fi
if [ ! -f "${START_TIME_FILE}" ]; then
    date +%s > "${START_TIME_FILE}"
fi


NEXT_TIME=60
function exit_at_45() {
    if [ -f "${START_TIME_FILE}" ]; then
	NOW=$(date +%s)
	START=$(cat "${START_TIME_FILE}")
	TIMEOUT_SECS=2700 # 45 * 60
	TIME=$((NOW - START))
	if (( TIME > NEXT_TIME )); then
	    echo "Build has taken $((TIME / 60)) minutes: $1"
	    NEXT_TIME=$((TIME + 60))
	fi
	if [ "$TIMEOUT" = true ] && (( TIME > TIMEOUT_SECS )); then
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
    while (ps -p "${PID}" > /dev/null); do
	exit_at_45 "$2"
	sleep "${SLEEP}"
    done
    # restore the xtrace setting
    if [ "${TRACE_STATUS}" -eq 0 ]; then
	set -o xtrace
    fi
    wait "$PID"
    return $?
}

# run a command ($1) in the background
# logging its stdout and stderr to $2
# and wait until it completed
function background() {
    $1 >> "$2" 2>&1 &
    BACKGROUND_PID=$!
    wait_and_echo $BACKGROUND_PID "$3"
}

function cleanup() {
    rv=$?
    # cat the log file if it exists
    if [ -f "$2" ]; then
	echo
	echo "Output of log file $2"
	echo
	tail -c $THREE_MB "$1"
	echo
    fi
    # delete the build directory
    rm -rf "$1"
    echo "Build failed. Removing $1"
    exit $rv
}

function cat_and_exit() {
    rv=$?
    # cat the log file if it exists
    if [ -f "$1" ]; then
	echo
	echo "Output of log file $1"
	echo
	# log size is limited to 4MB on travis
	# so hopefully the last 3MB is enough
	# to debug whatever went wrong
	tail -c $THREE_MB "$1"
	echo
    fi
    exit $rv
}

function brew_if_not_installed() {
    if ! brew ls | grep $1 --quiet; then
	brew install $1
    fi
}

function install_brew_packages() {
    brew update > /dev/null
    brew_if_not_installed autoconf
    brew_if_not_installed automake
    # something weird happened where glibtoolize was failing to find
    # sed, and reinstalling fixes it.
    brew reinstall -s libtool
    brew_if_not_installed pkg-config
    brew_if_not_installed protobuf
    brew_if_not_installed gmp
}

function install_apt_packages() {
    if [ -z "${TRAVIS+x}" ]; then
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
    if [ "${OS_NAME}" = "osx" ]; then
	install_brew_packages
    else
	install_apt_packages
    fi
    
    if [ "$CLEAN" = true ]; then
	rm -rf "${LBRYCRD_DEPENDENCIES}"
	rm -rf "${OUTPUT_DIR}"
    fi

    if [ ! -d "${LBRYCRD_DEPENDENCIES}" ]; then
       git clone https://github.com/lbryio/lbrycrd-dependencies.git "${LBRYCRD_DEPENDENCIES}"
    fi
    # TODO: if the repo exists, make sure its clean: revert to head.
    mkdir -p "${LOG_DIR}"

    build_dependency "${BDB_PREFIX}" "${LOG_DIR}/bdb_build.log" build_bdb
    build_dependency "${OPENSSL_PREFIX}" "${LOG_DIR}/openssl_build.log" build_openssl
    
    set +u
    export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig/"
    set -u

    build_dependency "${BOOST_PREFIX}" "${LOG_DIR}/boost_build.log" build_boost
    build_dependency "${LIBEVENT_PREFIX}" "${LOG_DIR}/libevent_build.log" build_libevent
}

function build_bdb() {
    BDB_LOG="$1"
    if [ "${OS_NAME}" = "osx" ]; then
	patch db-4.8.30.NC/dbinc/atomic.h < atomic.patch
    fi
    cd db-4.8.30.NC/build_unix
    echo "Building bdb.  tail -f $BDB_LOG to see the details and monitor progress"
    ../dist/configure --prefix="${BDB_PREFIX}" --enable-cxx --disable-shared --with-pic > "${BDB_LOG}"
    background make "${BDB_LOG}" "Waiting for bdb to finish building"
    make install >> "${BDB_LOG}" 2>&1
}

function build_openssl() {
    OPENSSL_LOG="$1"
    mkdir -p "${OPENSSL_PREFIX}/ssl"
    cd openssl-1.0.1p
    echo "Building openssl.  tail -f $OPENSSL_LOG to see the details and monitor progress"
    if [ "${OS_NAME}" = "osx" ]; then
	./Configure --prefix="${OPENSSL_PREFIX}" --openssldir="${OPENSSL_PREFIX}/ssl" \
		    -fPIC darwin64-x86_64-cc \
		    no-shared no-dso no-engines > "${OPENSSL_LOG}"
    else
	./Configure --prefix="${OPENSSL_PREFIX}" --openssldir="${OPENSSL_PREFIX}/ssl" \
		    linux-x86_64 -fPIC -static no-shared no-dso > "${OPENSSL_LOG}"
    fi
    background make "${OPENSSL_LOG}" "Waiting for openssl to finish building"
    make install >> "${OPENSSL_LOG}" 2>&1
}

function build_boost() {
    BOOST_LOG="$1"
    cd boost_1_59_0
    echo "Building Boost.  tail -f ${BOOST_LOG} to see the details and monitor progress"
    ./bootstrap.sh --prefix="${BOOST_PREFIX}" > "${BOOST_LOG}" 2>&1
    background "./b2 link=static cxxflags=-fPIC install" \
	       "${BOOST_LOG}" \
	       "Waiting for boost to finish building"
}

function build_libevent() {
    LIBEVENT_LOG="$1"
    if [ ! -d libevent ]; then
	git clone https://github.com/libevent/libevent.git
    fi
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
	trap 'cleanup "${PREFIX}" "${LOG}"' INT TERM EXIT
	cd "${LBRYCRD_DEPENDENCIES}"
	mkdir -p "${PREFIX}"
	"${BUILD}" "${LOG}"
	trap - INT TERM EXIT
    fi
}

function build_lbrycrd() {
    if [ "$CLONE" == true ]; then
	cd "${LBRYCRD_DEPENDENCIES}"
	git clone https://github.com/lbryio/lbrycrd
	cd lbrycrd
    else
	cd "${SOURCE_DIR}"
    fi
    ./autogen.sh > "${LBRYCRD_LOG}" 2>&1
    LDFLAGS="-L${OPENSSL_PREFIX}/lib/ -L${BDB_PREFIX}/lib/ -L${LIBEVENT_PREFIX}/lib/ -static-libstdc++"
    CPPFLAGS="-I${OPENSSL_PREFIX}/include -I${BDB_PREFIX}/include -I${LIBEVENT_PREFIX}/include/"
    if [ "${OS_NAME}" = "osx" ]; then
	OPTIONS="--enable-cxx --enable-static --disable-shared --with-pic"
    else
	OPTIONS=""
    fi
    ./configure --without-gui ${OPTIONS} \
		--with-boost="${BOOST_PREFIX}" \
		LDFLAGS="${LDFLAGS}" \
		CPPFLAGS="${CPPFLAGS}" >> "${LBRYCRD_LOG}" 2>&1
    background make "${LBRYCRD_LOG}" "Waiting for lbrycrd to finish building"
    # tests don't work on OSX. Should definitely figure out why
    # that is but, for now, not letting that stop the rest
    # of the build
    if [ "${OS_NAME}" = "linux" ]; then
	src/test/test_lbrycrd
    fi
    strip src/lbrycrdd
    strip src/lbrycrd-cli
    strip src/lbrycrd-tx
    strip src/test/test_lbrycrd
}

# these variables are needed in both functions
LBRYCRD_DEPENDENCIES="$(pwd)/lbrycrd-dependencies"
OUTPUT_DIR="$(pwd)/build"
LOG_DIR="$(pwd)/logs"
BDB_PREFIX="${OUTPUT_DIR}/bdb"
OPENSSL_PREFIX="${OUTPUT_DIR}/openssl"
BOOST_PREFIX="${OUTPUT_DIR}/boost"
LIBEVENT_PREFIX="${OUTPUT_DIR}/libevent"

if [ "${BUILD_DEPENDENCIES}" = true ]; then
    build_dependencies
fi

set +u
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig/:${LIBEVENT_PREFIX}/lib/pkgconfig/"
set -u

if [ "${BUILD_LBRYCRD}" = true ]; then
    LBRYCRD_LOG="${LOG_DIR}/lbrycrd_build.log"
    echo "Building lbrycrd.  tail -f ${LBRYCRD_LOG} to see the details and monitor progress"
    trap 'cat_and_exit "${LBRYCRD_LOG}"' INT TERM EXIT
    build_lbrycrd
    trap - INT TERM EXIT
fi
