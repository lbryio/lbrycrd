#!/usr/bin/env python3

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

    claimsintrie = json.load(sys.stdin)
    i = 0
    total_i = len(claimsintrie)
    print('Importing ' + str(total_i) + ' names. Progress:')
    for node in claimsintrie:
        if 'name' in node and 'claims' in node:
            prev_amount = sys.float_info.max
            for claim in node['claims']:
                amount = claim['amount'] if claim['amount'] < prev_amount else prev_amount / 2.0
                if amount <= 0.0:
                    amount = 0.0000001
                prev_amount = amount

                # ensure that we have enough LBC to cover that amount:
                result = sp.run(cli_tool + ['getbalance'], stdout=sp.PIPE, universal_newlines=True)
                balance = float(result.stdout)
                while balance < amount + 10.0:  # assuming no fees greater than 10
                    needed_blocks = math.ceil(amount + 100.0 - balance)
                    sp.run(cli_tool + ['generate', str(needed_blocks)], stdout=sp.PIPE, universal_newlines=True)
                    result = sp.run(cli_tool + ['getbalance'], stdout=sp.PIPE, universal_newlines=True)
                    balance = float(result.stdout)

                # insert the claim:
                name = node['name'].replace('\x00', '')  # not dealing with encodings today
                value = claim['value'].replace('\x00', '')
                result = sp.run(cli_tool + ['claimname', name, value, "{:17.8f}".format(amount)],
                                stdout=sp.PIPE, universal_newlines=True)
                txid = result.stdout
                if not txid or 'error' in txid:
                    print('Failed to create claim named ' + node['name'] +
                          '. Message: ' + txid + ', Amount: ' + str(amount), file=sys.stderr)
                    sys.exit(3)
        i += 1
        workdone = i / total_i
        print('\r[{0:20s}] {1:.1f}%, {2}'.format('#' * int(workdone * 20), workdone * 100, i), end='', flush=True)

    sp.run(cli_tool + ['generate', '20'], stdout=sp.PIPE, universal_newlines=True)  # far enough for activation
    print()


if __name__ == '__main__':
    main()
