#!/bin/bash

set -euox pipefail

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
    echo "-f: check formatting of committed code relative to master"
    echo "-r: remove intermediate files."
    echo "-l: build only lbrycrd"
    echo "-d: build only the dependencies"
    echo "-o: timeout build after 40 minutes"
    echo "-t: turn trace on"
    echo "-h: show help"
    exit 1
}

CLEAN=false
CHECK_CODE_FORMAT=false
BUILD_DEPENDENCIES=true
BUILD_LBRYCRD=true
TIMEOUT=false
THREE_MB=3145728
# this flag gets set to False if
# the script exits due to a timeout
OUTPUT_LOG=true

while getopts :crfldoth:w:d: FLAG; do
    case $FLAG in
	r)
	    CLEAN=true
	    ;;
	f)
	    CHECK_CODE_FORMAT=true
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

if [ "$(basename "$PWD")" != "lbrycrd" ]; then
    echo "Not currently in the lbrycrd directory. Cowardly refusing to go forward"
    exit 1
fi
SOURCE_DIR=$PWD

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
rm -f ${START_TIME_FILE}
date +%s > ${START_TIME_FILE}

NEXT_TIME=60
function exit_at_60() {
    if [ -f "${START_TIME_FILE}" ]; then
	NOW=$(date +%s)
	START=$(cat "${START_TIME_FILE}")
	TIMEOUT_SECS=3600 # 60 * 60
	TIME=$((NOW - START))
	if (( TIME > NEXT_TIME )); then
	    echo "Build has taken $((TIME / 60)) minutes: $1"
	    NEXT_TIME=$((TIME + 60))
	fi
	if [ "$TIMEOUT" = true ] && (( TIME > TIMEOUT_SECS )); then
	    echo 'Exiting at 60 minutes to allow the cache to populate'
	    OUTPUT_LOG=false
	    exit 1
	fi
    fi
}

# two arguments
#  - pid (probably from $!)
#  - echo message
function wait_and_echo() {
    PID=$1
    TIME=0
    SLEEP=3
    # loop until the process is no longer running
    # check every $SLEEP seconds, echoing a message every minute
    while (ps -p "${PID}" > /dev/null); do
	exit_at_60 "$2"
	sleep "${SLEEP}"
    done
}

# run a command ($1) in the background
# logging its stdout and stderr to $2
# and wait until it completed
function background() {
    eval $1 >> "$2" 2>&1 &
    BACKGROUND_PID=$!
    (
        set +xe # do not echo each sleep call in trace mode
        wait_and_echo $BACKGROUND_PID "$3"
    )
    wait $BACKGROUND_PID
}

function cleanup() {
    rv=$?
    if [ $rv -eq 0 ]; then
        return $rv
    fi
    # cat the log file if it exists
    if [ -f "$2" ] && [ "${OUTPUT_LOG}" = true ]; then
	echo
	echo "Output of log file $2"
	echo
        cat "$2"
#	tail -n 200 "$2"
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
    if [ -f "$1" ] && [ "${OUTPUT_LOG}" = true ]; then
	echo
	echo "Output of log file $1"
	echo
	# This used to be the last 3MB but outputing that
	# caused problems on travis.
	# Hopefully the last 1000 lines is enough
	# to debug whatever went wrong
	tail -n 1000 "$1"
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
    brew unlink python
    brew_if_not_installed autoconf
    brew_if_not_installed automake
    # something weird happened where glibtoolize was failing to find
    # sed, and reinstalling fixes it.
    brew reinstall libtool
    brew_if_not_installed pkg-config
    brew_if_not_installed protobuf
    brew_if_not_installed gmp

    if [ "${CHECK_CODE_FORMAT}" = true ]; then
        brew_if_not_installed clang-format
    fi
}

function install_apt_packages() {
    if [ -d "${OUTPUT_DIR}" ]; then
        return 0
    fi

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

    if [ "${CHECK_CODE_FORMAT}" = true ]; then
        $SUDO apt-get ${QUIET} install -y --no-install-recommends \
            clang-format-3.9
    fi

}

function build_dependencies() {
    if [ "${OS_NAME}" = "osx" ]; then
        PARALLEL="-j $(sysctl -n hw.ncpu)"
	install_brew_packages
    else
        PARALLEL="-j $(grep -c processor /proc/cpuinfo)"
	install_apt_packages
    fi

    if [ "$CLEAN" = true ]; then
	rm -rf "${LBRYCRD_DEPENDENCIES}"
	rm -rf "${OUTPUT_DIR}"
    fi

    mkdir -p "${LBRYCRD_DEPENDENCIES}"

    # Download required dependencies (if not already present)
    pushd ${LBRYCRD_DEPENDENCIES} > /dev/null
    if [ ! -f db-4.8.30.NC.zip ]; then
        wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.zip
        unzip -o -q db-4.8.30.NC.zip
    fi
    if [ ! -f libevent-2.1.8-stable.tar.gz ]; then
        wget https://github.com/libevent/libevent/releases/download/release-2.1.8-stable/libevent-2.1.8-stable.tar.gz
        tar -xzf libevent-2.1.8-stable.tar.gz
    fi
    if [ ! -f openssl-1.0.2r.tar.gz ]; then
        wget https://www.openssl.org/source/openssl-1.0.2r.tar.gz
        tar -xzf openssl-1.0.2r.tar.gz
    fi
    if [ ! -f icu4c-63_1-src.tgz ]; then
        wget http://download.icu-project.org/files/icu4c/63.1/icu4c-63_1-src.tgz
        tar -xzf icu4c-63_1-src.tgz
    fi
    if [ ! -f boost_1_64_0.tar.bz2 ]; then
        wget https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_64_0.tar.bz2
        tar -xjf boost_1_64_0.tar.bz2
    fi

    mkdir -p "${LOG_DIR}"

    build_dependency "${OPENSSL_PREFIX}" "${LOG_DIR}/openssl_build.log" build_openssl
    build_dependency "${ICU_PREFIX}" "${LOG_DIR}/icu_build.log" build_icu
    build_dependency "${BDB_PREFIX}" "${LOG_DIR}/bdb_build.log" build_bdb

    set +u
    export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig"
    set -u

    build_dependency "${BOOST_PREFIX}" "${LOG_DIR}/boost_build.log" build_boost
    build_dependency "${LIBEVENT_PREFIX}" "${LOG_DIR}/libevent_build.log" build_libevent
}

function build_bdb() {
    BDB_LOG="$1"
    if [ "${OS_NAME}" = "osx" ]; then
        # TODO: make this handle already patched files
	patch db-4.8.30.NC/dbinc/atomic.h < ../contrib/patches/atomic.patch
    fi
    cd db-4.8.30.NC/build_unix
    echo "Building bdb.  tail -f $BDB_LOG to see the details and monitor progress"
    ../dist/configure --prefix="${BDB_PREFIX}" --enable-cxx --disable-shared --with-pic > "${BDB_LOG}"
    background "make ${PARALLEL}" "${BDB_LOG}" "Waiting for bdb to finish building"
    make install >> "${BDB_LOG}" 2>&1
}

function build_openssl() {
    OPENSSL_LOG="$1"
    mkdir -p "${OPENSSL_PREFIX}/ssl"
    cd openssl-1.0.2r
    echo "Building openssl.  tail -f $OPENSSL_LOG to see the details and monitor progress"
    if [ "${OS_NAME}" = "osx" ]; then
	./Configure --prefix="${OPENSSL_PREFIX}" --openssldir="${OPENSSL_PREFIX}/ssl" \
		    -fPIC darwin64-x86_64-cc \
		    no-shared no-dso no-engines > "${OPENSSL_LOG}"
        make depend
    else
    [[ $(uname -m) = 'i686' ]] && OS_ARCH="linux-generic32" || OS_ARCH="linux-x86_64"
	./Configure --prefix="${OPENSSL_PREFIX}" --openssldir="${OPENSSL_PREFIX}/ssl" \
		    ${OS_ARCH} -fPIC -static no-shared no-dso > "${OPENSSL_LOG}"
    fi
    background "make ${PARALLEL}" "${OPENSSL_LOG}" "Waiting for openssl to finish building"
    make install >> "${OPENSSL_LOG}" 2>&1
}

function build_boost() {
    BOOST_LOG="$1"
    cd boost_1_64_0

    echo "int main() { return 0; }" > libs/regex/build/has_icu_test.cpp
    echo "int main() { return 0; }" > libs/locale/build/has_icu_test.cpp

    export BOOST_ICU_LIBS="-L${ICU_PREFIX}/lib -licui18n -licuuc -licudata -ldl"
    export BOOST_LIBRARIES="chrono,filesystem,program_options,system,locale,regex,thread,test"

    echo "Building Boost.  tail -f ${BOOST_LOG} to see the details and monitor progress"
    ./bootstrap.sh --prefix="${BOOST_PREFIX}" --with-icu="${ICU_PREFIX}" --with-libraries=${BOOST_LIBRARIES} > "${BOOST_LOG}" 2>&1
    b2cmd="./b2 --reconfigure ${PARALLEL} link=static cxxflags=\"-std=c++11 -fPIC\" install boost.locale.iconv=off boost.locale.posix=off -sICU_PATH=\"${ICU_PREFIX}\" -sICU_LINK=\"${BOOST_ICU_LIBS}\""
    background "${b2cmd}" "${BOOST_LOG}" "Waiting for boost to finish building"
}

function build_icu() {
    ICU_LOG="$1"
    mkdir -p "${ICU_PREFIX}/icu"
    pushd icu/source > /dev/null
    echo "Building icu.  tail -f $ICU_LOG to see the details and monitor progress"
    ./configure --prefix="${ICU_PREFIX}" --enable-draft --enable-tools \
                    --disable-shared --enable-static --disable-extras --disable-icuio --disable-dyload \
                    --disable-layout --disable-layoutex --disable-tests --disable-samples CFLAGS=-fPIC CPPFLAGS=-fPIC > "${ICU_LOG}"
    if [ ! -z ${TARGET+x} ]; then
        TMP_TARGET="${TARGET}"
        unset TARGET
    fi
    set +e
    background "make ${PARALLEL} VERBOSE=1" "${ICU_LOG}" "Waiting for icu to finish building"
    make install >> "${ICU_LOG}" 2>&1
    if [ ! -z ${TARGET+x} ]; then
        TARGET="${TMP_TARGET}"
    fi
    set -e
    popd > /dev/null
}

function build_libevent() {
    LIBEVENT_LOG="$1"
    cd libevent-2.1.8-stable
    echo "Building libevent.  tail -f ${LIBEVENT_LOG} to see the details and monitor progress"
    ./autogen.sh > "${LIBEVENT_LOG}" 2>&1
    ./configure --prefix="${LIBEVENT_PREFIX}" --enable-static --disable-shared --with-pic \
		LDFLAGS="-L${OPENSSL_PREFIX}/lib" \
		CPPFLAGS="-I${OPENSSL_PREFIX}/include" >> "${LIBEVENT_LOG}" 2>&1
    background "make ${PARALLEL}" "${LIBEVENT_LOG}" "Waiting for libevent to finish building"
    make install >> "${LIBEVENT_LOG}"
}

function build_dependency() {
    pushd .
    PREFIX=$1
    LOG=$2
    BUILD=$3
    cd "${LBRYCRD_DEPENDENCIES}"
    mkdir -p "${PREFIX}"
    trap 'cleanup "${PREFIX}" "${LOG}"' INT TERM EXIT
	"${BUILD}" "${LOG}"
	trap - INT TERM EXIT
    popd
}

function build_lbrycrd() {
    cd "${SOURCE_DIR}"
    ./autogen.sh > "${LBRYCRD_LOG}" 2>&1
    LDFLAGS="-L${OPENSSL_PREFIX}/lib -L${BDB_PREFIX}/lib -L${LIBEVENT_PREFIX}/lib"
    OPTIONS="--enable-cxx --enable-static --disable-shared --with-pic"
    if [ "${OS_NAME}" = "osx" ]; then
        CPPFLAGS="-I${OPENSSL_PREFIX}/include -I${BDB_PREFIX}/include -I${LIBEVENT_PREFIX}/include -I${ICU_PREFIX}/include"
    else
        CPPFLAGS="-I${OPENSSL_PREFIX}/include -I${BDB_PREFIX}/include -I${LIBEVENT_PREFIX}/include -I${ICU_PREFIX}/include -Wno-unused-local-typedefs -Wno-deprecated -Wno-implicit-fallthrough"
    fi

    CPPFLAGS="${CPPFLAGS}" LDFLAGS="${LDFLAGS}"  \
            ./configure --without-gui ${OPTIONS} \
	    --with-boost="${BOOST_PREFIX}"   \
            --with-icu="${ICU_PREFIX}" >> "${LBRYCRD_LOG}" 2>&1

    background "make ${PARALLEL}" "${LBRYCRD_LOG}" "Waiting for lbrycrd to finish building"
}

function clang_format_diff(){
    # run a code formatting check on any commits not in master
    # requires clang-format
    git diff -U0 origin/master -- '*.h' '*.cpp' | ./contrib/devtools/clang-format-diff.py -p1
}

# these variables are needed in both functions
LBRYCRD_DEPENDENCIES="$(pwd)/lbrycrd-dependencies"
OUTPUT_DIR="$(pwd)/build"
LOG_DIR="$(pwd)/logs"
ICU_PREFIX="${OUTPUT_DIR}/icu"
BDB_PREFIX="${OUTPUT_DIR}/bdb"
OPENSSL_PREFIX="${OUTPUT_DIR}/openssl"
BOOST_PREFIX="${OUTPUT_DIR}/boost"
LIBEVENT_PREFIX="${OUTPUT_DIR}/libevent"

if [ "${BUILD_DEPENDENCIES}" = true ]; then
    build_dependencies
fi

if [ "${CHECK_CODE_FORMAT}" = true ]; then
    LINES_W_FORMAT_REQUIRED=$(clang_format_diff | wc -l)
    if [ ${LINES_W_FORMAT_REQUIRED} -ne 0  ]; then
        echo "Failed to pass clang format diff: See below for the diff"
        clang_format_diff
        exit 1
    fi
fi

set +u
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${OPENSSL_PREFIX}/lib/pkgconfig:${LIBEVENT_PREFIX}/lib/pkgconfig:${ICU_PREFIX}/lib/pkgconfig"
set -u

if [ "${BUILD_LBRYCRD}" = true ]; then
    LBRYCRD_LOG="${LOG_DIR}/lbrycrd_build.log"
    echo "Building lbrycrd.  tail -f ${LBRYCRD_LOG} to see the details and monitor progress"
    trap 'cat_and_exit "${LBRYCRD_LOG}"' INT TERM EXIT
    build_lbrycrd
    trap - INT TERM EXIT

    ./src/test/test_lbrycrd
    set +u
    if [[ ! $CXXFLAGS =~ -g ]]; then
        strip src/lbrycrdd
        strip src/lbrycrd-cli
        strip src/lbrycrd-tx
    fi
fi
