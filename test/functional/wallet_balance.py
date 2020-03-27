#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet balance RPC methods."""
from decimal import Decimal
import struct

from test_framework.address import ADDRESS_BCRT1_UNSPENDABLE as ADDRESS_WATCHONLY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes,
    sync_blocks,
)


def create_transactions(node, address, amt, fees):
    # Create and sign raw transactions from node to address for amt.
    # Creates a transaction for each fee and returns an array
    # of the raw transactions.
    unspents = node.listunspent(0)
    utxos = [u for u in unspents if u['spendable'] and u['safe']]

    # Create transactions
    inputs = []
    ins_total = 0
    for utxo in utxos:
        inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
        ins_total += utxo['amount']
        if ins_total >= amt + max(fees):
            break
    # make sure there was enough utxos
    assert ins_total >= amt + max(fees)

    txs = []
    for fee in fees:
        outputs = {address: amt}
        # prevent 0 change output
        if ins_total > amt + fee:
            outputs[node.getrawchangeaddress()] = ins_total - amt - fee
        raw_tx = node.createrawtransaction(inputs, outputs, 0, True)
        raw_tx = node.signrawtransactionwithwallet(raw_tx)
        assert_equal(raw_tx['complete'], True)
        txs.append(raw_tx)

    return txs

class WalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            ['-limitdescendantcount=3',
             '-segwitheight=0',  # Limit mempool descendants as a hack to have wallet txs rejected from the mempool
             '-whitelist=127.0.0.1'],
            ['-segwitheight=0',
             '-whitelist=127.0.0.1']
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.nodes[0].importaddress(ADDRESS_WATCHONLY)
        # Check that nodes don't own any UTXOs
        assert_equal(len(self.nodes[0].listunspent()), 0)
        assert_equal(len(self.nodes[1].listunspent()), 0)

        self.log.info("Check that only node 0 is watching an address")
        assert 'watchonly' in self.nodes[0].getbalances()
        assert 'watchonly' not in self.nodes[1].getbalances()

        self.log.info("Mining blocks ...")
        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[1].generate(1)
        self.nodes[1].generatetoaddress(101, ADDRESS_WATCHONLY)
        self.sync_all()

        assert_equal(self.nodes[0].getbalances()['mine']['trusted'], 1)
        assert_equal(self.nodes[0].getwalletinfo()['balance'], 1)
        assert_equal(self.nodes[1].getbalances()['mine']['trusted'], 1)

        assert_equal(self.nodes[0].getbalances()['watchonly']['immature'], 100)
        assert 'watchonly' not in self.nodes[1].getbalances()


        self.log.info("Test getbalance with different arguments")
        assert_equal(self.nodes[0].getbalance("*"), 1)
        assert_equal(self.nodes[0].getbalance("*", 1), 1)
        assert_equal(self.nodes[0].getbalance("*", 1, True), 2)
        assert_equal(self.nodes[0].getbalance(minconf=1), 1)
        assert_equal(self.nodes[0].getbalance(minconf=0, include_watchonly=True), 2)
        assert_equal(self.nodes[1].getbalance(minconf=0, include_watchonly=True), 1)

        # Send 40 BTC from 0 to 1 and 60 BTC from 1 to 0.
        txs = create_transactions(self.nodes[0], self.nodes[1].getnewaddress(), Decimal('0.4'), [Decimal('0.01')])
        self.nodes[0].sendrawtransaction(txs[0]['hex'])
        self.nodes[1].sendrawtransaction(txs[0]['hex'])  # sending on both nodes is faster than waiting for propagation

        self.sync_all()
        txs = create_transactions(self.nodes[1], self.nodes[0].getnewaddress(), Decimal('0.6'), [Decimal('0.01'), Decimal('0.02')])
        self.nodes[1].sendrawtransaction(txs[0]['hex'])
        self.nodes[0].sendrawtransaction(txs[0]['hex'])  # sending on both nodes is faster than waiting for propagation
        self.sync_all()

        # First argument of getbalance must be set to "*"
        assert_raises_rpc_error(-32, "dummy first argument must be excluded or set to \"*\"", self.nodes[1].getbalance, "")

        self.log.info("Test getbalance and getunconfirmedbalance with unconfirmed inputs")

        def test_balances(*, fee_node_1=0):
            # getbalance without any arguments includes unconfirmed transactions, but not untrusted transactions
            assert_equal(self.nodes[0].getbalance(), Decimal('0.59'))  # change from node 0's send
            assert_equal(self.nodes[1].getbalance(), Decimal('0.4') - fee_node_1)  # change from node 1's send
            # Same with minconf=0
            assert_equal(self.nodes[0].getbalance(minconf=0), Decimal('0.59'))
            assert_equal(self.nodes[1].getbalance(minconf=0), Decimal('0.4') - fee_node_1)
            # getbalance with a minconf incorrectly excludes coins that have been spent more recently than the minconf blocks ago
            # TODO: fix getbalance tracking of coin spentness depth
            assert_equal(self.nodes[0].getbalance(minconf=1), Decimal('0.00'))
            assert_equal(self.nodes[1].getbalance(minconf=1), Decimal('0.00'))
            # getunconfirmedbalance
            assert_equal(self.nodes[0].getunconfirmedbalance(), Decimal('0.6'))  # output of node 1's spend
            assert_equal(self.nodes[0].getbalances()['mine']['untrusted_pending'], Decimal('0.6'))
            assert_equal(self.nodes[0].getwalletinfo()["unconfirmed_balance"], Decimal('0.6'))

            assert_equal(self.nodes[1].getunconfirmedbalance(), Decimal('0.4'))  # Doesn't include output of node 0's send since it was spent
            assert_equal(self.nodes[1].getbalances()['mine']['untrusted_pending'], Decimal('0.4'))
            assert_equal(self.nodes[1].getwalletinfo()["unconfirmed_balance"], Decimal('0.4'))

        test_balances(fee_node_1=Decimal('0.01'))

        # Node 1 bumps the transaction fee and resends
        self.nodes[1].sendrawtransaction(txs[1]['hex'])
        self.nodes[0].sendrawtransaction(txs[1]['hex'])  # sending on both nodes is faster than waiting for propagation
        self.sync_all()

        self.log.info("Test getbalance and getunconfirmedbalance with conflicted unconfirmed inputs")
        test_balances(fee_node_1=Decimal('0.02'))

        self.nodes[1].generatetoaddress(1, ADDRESS_WATCHONLY)
        self.sync_all()

        # balances are correct after the transactions are confirmed
        assert_equal(self.nodes[0].getbalance(), Decimal('1.19'))  # node 1's send plus change from node 0's send
        assert_equal(self.nodes[1].getbalance(), Decimal('0.78'))  # change from node 0's send

        # Send total balance away from node 1
        txs = create_transactions(self.nodes[1], self.nodes[0].getnewaddress(), Decimal('0.77'), [Decimal('0.01')])
        self.nodes[1].sendrawtransaction(txs[0]['hex'])
        self.nodes[1].generatetoaddress(2, ADDRESS_WATCHONLY)
        self.sync_all()

        # getbalance with a minconf incorrectly excludes coins that have been spent more recently than the minconf blocks ago
        # TODO: fix getbalance tracking of coin spentness depth
        # getbalance with minconf=3 should still show the old balance
        assert_equal(self.nodes[1].getbalance(minconf=3), Decimal('0'))

        # getbalance with minconf=2 will show the new balance.
        assert_equal(self.nodes[1].getbalance(minconf=2), Decimal('0'))

        # check mempool transactions count for wallet unconfirmed balance after
        # dynamically loading the wallet.
        before = self.nodes[1].getunconfirmedbalance()
        dst = self.nodes[1].getnewaddress()
        self.nodes[1].unloadwallet('')
        self.nodes[0].sendtoaddress(dst, 0.1)
        self.sync_all()
        self.nodes[1].loadwallet('')
        after = self.nodes[1].getunconfirmedbalance()
        assert_equal(before + Decimal('0.1'), after)

        # Create 3 more wallet txs, where the last is not accepted to the
        # mempool because it is the third descendant of the tx above
        for _ in range(3):
            # Set amount high enough such that all coins are spent by each tx
            txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 1.5)

        self.log.info('Check that wallet txs not in the mempool are untrusted')
        assert txid not in self.nodes[0].getrawmempool()
        assert_equal(self.nodes[0].gettransaction(txid)['trusted'], False)
        assert_equal(self.nodes[0].getbalance(minconf=0), 0)

        self.log.info("Test replacement and reorg of non-mempool tx")
        tx_orig = self.nodes[0].gettransaction(txid)['hex']
        # Increase fee by 1 coin
        tx_replace = tx_orig.replace(
            struct.pack("<q", 99 * 10**8).hex(),
            struct.pack("<q", 98 * 10**8).hex(),
        )
        tx_replace = self.nodes[0].signrawtransactionwithwallet(tx_replace)['hex']
        # Total balance is given by the sum of outputs of the tx
        total_amount = sum([o['value'] for o in self.nodes[0].decoderawtransaction(tx_replace)['vout']])
        self.sync_all()
        self.nodes[1].sendrawtransaction(hexstring=tx_replace, maxfeerate=0)

        # Now confirm tx_replace
        block_reorg = self.nodes[1].generatetoaddress(1, ADDRESS_WATCHONLY)[0]
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(minconf=0), total_amount)

        self.log.info('Put txs back into mempool of node 1 (not node 0)')
        self.nodes[0].invalidateblock(block_reorg)
        self.nodes[1].invalidateblock(block_reorg)
        self.sync_blocks()
        self.nodes[0].syncwithvalidationinterfacequeue()
        assert_equal(self.nodes[0].getbalance(minconf=0), 0)  # wallet txs not in the mempool are untrusted
        self.nodes[0].generatetoaddress(1, ADDRESS_WATCHONLY)
        assert_equal(self.nodes[0].getbalance(minconf=0), 0)  # wallet txs not in the mempool are untrusted

        # Now confirm tx_orig
        self.restart_node(1, ['-persistmempool=0', '-segwitheight=0', '-whitelist=127.0.0.1'])
        connect_nodes(self.nodes[0], 1)
        sync_blocks(self.nodes)
        self.nodes[1].sendrawtransaction(tx_orig)
        self.nodes[1].generatetoaddress(1, ADDRESS_WATCHONLY)
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(minconf=0), total_amount)  # The reorg recovered our fee of 1 coin


if __name__ == '__main__':
    WalletTest().main()
