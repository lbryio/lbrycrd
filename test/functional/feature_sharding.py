#!/usr/bin/env python3
# Copyright (c) 2026 The LBRY developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test block storage sharding (-shard, -shardsegment).

Pruned nodes with sharding keep some segments of the block chain, advertise
them (NODE_SHARD and a shardinfo message) and serve them, so that a set of
sharded nodes can provide the whole chain to a node syncing from scratch.

  node0: full node that mines the chain
  node1: pruned, keeps segments 0, 2, 4 and 6 (6 is the incomplete segment at the tip)
  node2: pruned, keeps segments 1, 3 and 5
  node3: new node that syncs from node1 and node2 only
  node4: pruned, -shard=100 (random selection that keeps every segment)
"""
import os
import time

from test_framework.messages import (
    CInv,
    msg_getdata,
    NODE_NETWORK,
    NODE_NETWORK_LIMITED,
    NODE_SHARD,
    NODE_WITNESS,
)
from test_framework.mininode import P2PInterface, mininode_lock
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    wait_until,
)

SEGMENT = 1000
# Segments 0-5 are complete and segment 6 holds heights 6000-6400. Pruned
# nodes keep the last 288 blocks, so heights 6000-6112 are only served as
# part of segment 6.
CHAIN_HEIGHT = 6400
TIP_SEGMENT = CHAIN_HEIGHT // SEGMENT
SHARD_ARGS = ['-prune=1', '-fastprune', '-shardsegmentsize=%d' % SEGMENT]
NODE1_SEGMENTS = [0, 2, 4, 6]
NODE2_SEGMENTS = [1, 3, 5]


class P2PShardClient(P2PInterface):
    def on_inv(self, message):
        pass

    def wait_for_shardinfo(self, timeout=10):
        wait_until(lambda: self.last_message.get("shardinfo"), timeout=timeout, lock=mininode_lock)
        with mininode_lock:
            return self.last_message["shardinfo"]

    def send_getdata_for_block(self, blockhash):
        self.send_message(msg_getdata([CInv(2, int(blockhash, 16))]))


class ShardingTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 5
        self.rpc_timeout = 240
        self.extra_args = [
            [],
            SHARD_ARGS + ['-shardsegment=%d' % s for s in NODE1_SEGMENTS],
            SHARD_ARGS + ['-shardsegment=%d' % s for s in NODE2_SEGMENTS],
            [],
            SHARD_ARGS + ['-shard=100'],
        ]

    def setup_network(self):
        # Nodes are connected after mining: a node syncing while blocks are
        # mined answers every block announcement with a getheaders, and
        # re-validating thousands of LBRY headers each time is very slow.
        self.setup_nodes()

    def middle_of(self, segment):
        return segment * SEGMENT + SEGMENT // 2

    def has_block(self, node, height):
        blockhash = self.nodes[0].getblockhash(height)
        try:
            node.getblock(blockhash)
            return True
        except Exception as e:
            assert 'pruned data' in str(e), str(e)
            return False

    def run_test(self):
        node0, node1, node2, node3, node4 = self.nodes

        self.log.info("Sharded nodes advertise NODE_SHARD with NODE_NETWORK_LIMITED, not NODE_NETWORK")
        services = int(node1.getnetworkinfo()['localservices'], 16)
        assert_equal(services, NODE_NETWORK_LIMITED | NODE_WITNESS | NODE_SHARD)
        assert 'SHARD' in node1.getnetworkinfo()['localservicesnames']
        assert_equal(int(node0.getnetworkinfo()['localservices'], 16) & NODE_SHARD, 0)

        self.log.info("Mine %d blocks and sync the pruned nodes" % CHAIN_HEIGHT)
        address = node0.get_deterministic_priv_key().address
        while node0.getblockcount() < CHAIN_HEIGHT:
            node0.generatetoaddress(min(1000, CHAIN_HEIGHT - node0.getblockcount()), address)
        # node3 stays unconnected until it syncs from the sharded nodes.
        for i in (1, 2, 4):
            connect_nodes(self.nodes[i], 0)
        self.sync_blocks([node0, node1, node2, node4], timeout=300)

        self.log.info("Prune: files holding kept segments survive")
        for node in (node1, node2, node4):
            node.pruneblockchain(CHAIN_HEIGHT)
        # node4 keeps every segment; node1 and node2 keep about 60% and 45% of the chain.
        full_size = node4.getblockchaininfo()['size_on_disk']
        assert node1.getblockchaininfo()['size_on_disk'] < full_size * 0.8
        assert node2.getblockchaininfo()['size_on_disk'] < full_size * 0.7

        for segment in range(TIP_SEGMENT):
            height = self.middle_of(segment)
            assert_equal(self.has_block(node1, height), segment in NODE1_SEGMENTS)
            assert_equal(self.has_block(node2, height), segment in NODE2_SEGMENTS)
            assert self.has_block(node4, height)
        # Heights 6000-6112 are older than the 288 blocks every pruned node keeps.
        assert self.has_block(node1, TIP_SEGMENT * SEGMENT + 50)

        self.log.info("getblockchaininfo reports the selection and the held segments")
        shard = node1.getblockchaininfo()['shard']
        assert_equal(shard['segment_size'], SEGMENT)
        assert_equal(shard['percent'], 0)
        assert_equal(shard['segments'], NODE1_SEGMENTS)
        assert_equal(shard['held_segments'], NODE1_SEGMENTS)
        assert_equal(node2.getblockchaininfo()['shard']['held_segments'], NODE2_SEGMENTS)
        shard = node4.getblockchaininfo()['shard']
        assert_equal(shard['percent'], 100)
        assert_equal(shard['held_segments'], list(range(TIP_SEGMENT + 1)))
        assert 'shard' not in node0.getblockchaininfo()

        self.log.info("-shard stores its random seed so the selection survives restarts")
        seed_path = os.path.join(node4.datadir, 'regtest', 'shardseed')
        with open(seed_path, encoding='utf8') as f:
            seed = f.read()
        self.restart_node(4)
        connect_nodes(node4, 0)
        with open(seed_path, encoding='utf8') as f:
            assert_equal(f.read(), seed)
        assert not os.path.exists(os.path.join(node1.datadir, 'regtest', 'shardseed'))

        self.log.info("A sharded node sends shardinfo and serves the segments it holds")
        peer = node1.add_p2p_connection(P2PShardClient())
        assert_equal(peer.nServices, NODE_NETWORK_LIMITED | NODE_WITNESS | NODE_SHARD)
        info = peer.wait_for_shardinfo()
        assert_equal(info.segment_size, SEGMENT)
        assert_equal(info.segments, NODE1_SEGMENTS)
        for height in (self.middle_of(0), TIP_SEGMENT * SEGMENT + 50):
            blockhash = node0.getblockhash(height)
            peer.send_getdata_for_block(blockhash)
            peer.wait_for_block(int(blockhash, 16), timeout=10)

        self.log.info("...and disconnects peers asking for old blocks outside them")
        peer.send_getdata_for_block(node0.getblockhash(self.middle_of(1)))
        peer.wait_for_disconnect(timeout=10)
        node1.disconnect_p2ps()

        self.log.info("A new node can't sync from node2 alone: nobody it knows has segment 0")
        assert_equal(node3.getblockcount(), 0)
        connect_nodes(node3, 2)
        wait_until(lambda: node3.getblockchaininfo()['headers'] == CHAIN_HEIGHT, timeout=60)
        wait_until(lambda: any(p.get('shard_segments') == NODE2_SEGMENTS for p in node3.getpeerinfo()), timeout=30)
        time.sleep(5)
        assert_equal(node3.getblockcount(), 0)

        self.log.info("With node1 as well it syncs the whole chain from sharded nodes only")
        connect_nodes(node3, 1)
        wait_until(lambda: node3.getblockcount() == CHAIN_HEIGHT, timeout=300)
        assert_equal(node3.getbestblockhash(), node0.getbestblockhash())
        for height in (0, self.middle_of(0), self.middle_of(1), self.middle_of(5), TIP_SEGMENT * SEGMENT + 50):
            assert_equal(node3.getblock(node0.getblockhash(height))['height'], height)
        segments = sorted(p['shard_segments'] for p in node3.getpeerinfo())
        assert_equal(segments, sorted([NODE1_SEGMENTS, NODE2_SEGMENTS]))
        for p in node3.getpeerinfo():
            assert_equal(p['shard_segment_size'], SEGMENT)
            assert_equal(int(p['services'], 16) & NODE_NETWORK, 0)

        self.log.info("New blocks reach the sharded nodes and the synced node as usual")
        node0.generatetoaddress(10, address)
        connect_nodes(node0, 3)
        self.sync_blocks()

        self.log.info("Sharding requires pruning, and checks its options")
        self.stop_node(3)
        node3.assert_start_raises_init_error(['-shard=10'], 'Error: Sharding (-shard or -shardsegment) requires pruning (-prune).')
        node3.assert_start_raises_init_error(['-prune=1', '-shard=101'], 'Error: -shard must be a percentage from 0 to 100 (got 101).')
        node3.assert_start_raises_init_error(['-prune=1', '-shardsegment=x'], "Error: Invalid -shardsegment 'x': expected a segment number (0 or more).")


if __name__ == '__main__':
    ShardingTest().main()
