#!/usr/bin/env bash

set -o pipefail

function HELP {
    echo "Use this command to build lbrycrd."
    echo "Dependencies will be pulled and built first."
    echo "Use autogen & configure directly to avoid this and use system shared libraries instead."
    echo
    echo "Optional arguments:"
    echo "-jN: number of parallel build jobs"
    echo "-q: compile the QT GUI (not working at present)"
    echo "-d: force a rebuild of dependencies"
    echo "-u: run the unit tests when done"
    echo "-g: include debug symbols"
    echo "-h: show help"
    exit 1
}

REBUILD_DEPENDENCIES=false
RUN_UNIT_TESTS=false
COMPILE_WITH_DEBUG=false
DO_NOT_COMPILE_THE_GUI="NO_QT=1"
WITH_COMPILE_THE_GUI=no

if test -z $PARALLEL_JOBS; then
    PARALLEL_JOBS=$(expr $(getconf _NPROCESSORS_ONLN) / 2 + 1)
fi

while getopts j:qdugh FLAG; do
  case ${FLAG} in
    j)
      PARALLEL_JOBS=$OPTARG
      ;;
    q)
      DO_NOT_COMPILE_THE_GUI=
      WITH_COMPILE_THE_GUI=qt5
      ;;
    g)
      COMPILE_WITH_DEBUG=true
      ;;
    u)
      RUN_UNIT_TESTS=true
      ;;
    d)
      REBUILD_DEPENDENCIES=true
      ;;
    h)
      HELP
      ;;
    \?)
      HELP
      ;;
  esac
done

echo "Compiling with ${PARALLEL_JOBS} jobs in parallel."

BUILD_FLAGS=(CXXFLAGS="-O3 -march=native")
if test "$COMPILE_WITH_DEBUG" = true; then
    BUILD_FLAGS=(--with-debug CXXFLAGS="-Og -g")
fi

cd depends
if test "$REBUILD_DEPENDENCIES" = true; then
    make clean
fi
make -j${PARALLEL_JOBS} ${DO_NOT_COMPILE_THE_GUI} V=1
cd ..

LC_ALL=C autoreconf --install

CONFIG_SITE=$(pwd)/depends/$($(pwd)/depends/config.guess)/share/config.site ./configure --enable-reduce-exports \
    --enable-static --disable-shared --with-pic --with-gui=${WITH_COMPILE_THE_GUI} "${BUILD_FLAGS[@]}"

if test $? -eq 0; then
    make -j${PARALLEL_JOBS}
fi

if test $? -eq 0 && "$RUN_UNIT_TESTS" = true; then
    ./src/test/test_lbrycrd
fi