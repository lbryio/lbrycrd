#!/usr/bin/env python3

import datetime
import json
import math
import subprocess as sp
import sys


def main():
    if len(sys.argv) < 2:  # pass in the CLI tool so that we don't have to replicate the cookie handling
        print('Missing required argument: <path to CLI tool> [CLI tool args...]', file=sys.stderr)
        sys.exit(1)

    cli_tool = sys.argv[1:]
    if '-regtest' not in cli_tool:
        cli_tool.append('-regtest')

    result = sp.run(cli_tool + ['getblockchaininfo'], stdout=sp.PIPE, universal_newlines=True)
    info = json.loads(result.stdout)
    if not info or info['chain'] != 'lbrycrdreg':
        print('Unable to query block chain info; are you missing the RPC authentication parameters?', file=sys.stderr)
        sys.exit(2)

    names = sys.stdin.readlines()
    i = 0
    total_i = len(names)
    print('Importing ' + str(total_i) + ' names. Progress:')
    # make sure we have neough LBC to start:
    amount = 1 / 400
    for name in names:

        # as noted below, we roll the block forward for every 100 names
        if i % 100 == 0:  # before adding any names ensure that we have enough mula to roll the block forward after
            result = sp.run(cli_tool + ['getbalance'], stdout=sp.PIPE, universal_newlines=True)
            balance = float(result.stdout)
            while balance < amount * 100.0 + 10.0:  # assuming no fees greater than 10
                needed_blocks = math.ceil(amount * 100.0 + 10.0 - balance)
                sp.run(cli_tool + ['generate', str(needed_blocks)], stdout=sp.PIPE, universal_newlines=True)
                result = sp.run(cli_tool + ['getbalance'], stdout=sp.PIPE, universal_newlines=True)
                balance = float(result.stdout)

        result = sp.run(cli_tool + ['claimname', name, str(datetime.datetime.utcnow().isoformat()),
                                    "{:17.8f}".format(amount)], stdout=sp.PIPE, universal_newlines=True)
        txid = result.stdout
        if not txid or 'error' in txid:
            print('Failed to create claim named ' + name +
                  '. Message: ' + txid + ', Amount: ' + str(amount), file=sys.stderr)
            sys.exit(3)

        i += 1
        if i % 100 == 0 or i == total_i:  # time to flush
            sp.run(cli_tool + ['generate', '1'], stdout=sp.PIPE, universal_newlines=True)

        workdone = i / total_i
        print('\r[{0:20s}] {1:.1f}%, {2}'.format('#' * int(workdone * 20), workdone * 100, i), end='', flush=True)

    print()


if __name__ == '__main__':
    main()
