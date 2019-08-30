#!/usr/bin/env bash

set -u -e -o pipefail
trap "kill 0" EXIT

"$(pwd)/test/util/bitcoin-util-test.py"
"$(pwd)/test/util/rpcauth-test.py"

"$(pwd)/test/lint/lint-filenames.sh"
"$(pwd)/test/lint/lint-include-guards.sh"
"$(pwd)/test/lint/lint-includes.sh"
"$(pwd)/test/lint/lint-tests.sh"
"$(pwd)/test/lint/lint-whitespace.sh"

# TODO: make the rest of these work:
#"$(pwd)/test/functional/test_runner.py" interface_rest

# do a little run-through of the basic CLI commands:
rm -fr /tmp/regtest_cli_testing
mkdir /tmp/regtest_cli_testing
CLI="$(pwd)/src/lbrycrd-cli -regtest -datadir=/tmp/regtest_cli_testing"
"$(pwd)/src/lbrycrdd" -regtest -txindex -datadir=/tmp/regtest_cli_testing -printtoconsole=0 &
sleep 3
while [[ "$($CLI help)" == *"Loading"* ]]; do sleep 1; done

GEN150=$($CLI generate 120)
TX=$($CLI claimname tester deadbeef 1)
GEN1=$($CLI generate 1)
LIST=$($CLI listnameclaims)
if [[ "$LIST" != *"$TX"* ]]; then exit 1; fi

