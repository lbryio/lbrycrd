# Block storage sharding

A pruned node (`-prune`) deletes old blocks, so it can't help a new node download the chain. The more operators prune, the fewer peers can serve the full history.

With sharding, a pruned node also keeps some parts of the historical chain and serves them to peers. A node syncing from scratch can then download the chain from several sharded nodes that each hold different parts.

Sharding only changes block storage and the peer-to-peer protocol. It doesn't change validation or consensus, and nodes without it ignore it.

## Usage

Sharding requires pruning. The chain is divided into segments of 10,000 blocks (heights `n*10000` to `(n+1)*10000-1`), about 17 days of LBRY blocks.

| Option | Meaning |
|---|---|
| `-shard=<percent>` | Keep about this percentage of segments (0–100), chosen at random. |
| `-shardsegment=<n>` | Keep segment `n`. Can be given several times and combined with `-shard`. |

For example, `lbrycrdd -prune=2000 -txindex=0 -shard=20` keeps 2 GB of recent blocks plus about a fifth of the history. lbrycrd enables `-txindex` by default, and pruning doesn't work with it, so `-txindex=0` is required.

- The random choice comes from a seed stored in `shardseed` in the data directory, so it stays the same across restarts.
- Kept segments are not counted towards the `-prune` target. The target limits everything else.
- Blocks that were pruned before sharding was enabled are not downloaded again. A segment is only kept and served if every block in it is still on disk. To keep a full selection, enable sharding before syncing, or on a node that has not pruned yet.
- `getblockchaininfo` shows the configuration and the segments held (`shard`). `getpeerinfo` shows the segments each sharded peer serves (`shard_segments`).

## Protocol

**Service bit.** A sharded node sets `NODE_SHARD` (bit 24) together with `NODE_NETWORK_LIMITED`, and without `NODE_NETWORK`. Bit 24 is in the range Bitcoin reserves for experiments, so it would need a proper allocation if other implementations adopt it.

**`shardinfo` message.** A sharded node sends `shardinfo` after the version handshake, and again whenever its list changes. The payload is:

| Field | Type | Meaning |
|---|---|---|
| segment size | `uint32` | Blocks per segment. Nodes don't need to use the same size. |
| segments | `vector<uint32>` | Sorted segment numbers, at most 100,000. |

The node serves every block of each listed segment, up to its tip. That includes the segment containing the tip, so that blocks older than the usual 288-block window are still served.

**Serving.** A sharded node serves blocks in its listed segments, plus its most recent 288 blocks like any pruned node. As with other pruned nodes, it disconnects peers that ask for older blocks outside those ranges.

**Downloading.** During initial sync a node normally downloads only from full (`NODE_NETWORK`) peers. It now also downloads from sharded peers that have sent `shardinfo`, and asks each one only for blocks it holds.

A peer is only treated as stalling the download if the waiting peer could have served the same block. Otherwise the stalling peer may be the only one with that segment, and disconnecting it would leave the sync stuck.

**Peer selection.** While syncing, a node uses at most half of its outbound connections for sharded peers. The rest go to full nodes, as before. Once synced, it treats sharded peers like any other pruned peer.

## Limitations

- **Gaps stall the sync.** If no reachable peer holds a segment, the sync stops at that segment until such a peer connects. When no blocks arrive for a while, the existing stale-tip check tries extra outbound peers. Sharding helps most when many nodes share the history, or a few full nodes remain.
- **DNS seeds.** Seeders that select nodes by service bit don't yet know about `NODE_SHARD`. Until they do, syncing nodes find sharded peers through address relay.
- **Peer selection is untested.** The half-of-outbound limit is not covered by the functional test, which uses manual connections.

## Tests

| Test | Covers |
|---|---|
| `src/test/shard_tests.cpp` | Segment selection, which block files are kept, which segments count as held |
| `test/functional/feature_sharding.py` | Pruning, advertising, serving and refusing blocks, and a new node syncing a 6,400-block regtest chain from two sharded nodes |

Two test-only options keep the regtest chain short. `-fastprune` uses 64 KiB block files, and `-shardsegmentsize` sets the segment size (regtest only).
