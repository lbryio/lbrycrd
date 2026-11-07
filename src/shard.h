// Copyright (c) 2026 The LBRY developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SHARD_H
#define BITCOIN_SHARD_H

#include <stdint.h>
#include <string>
#include <vector>

class CBlockFileInfo;
class CChain;

/**
 * Block storage sharding.
 *
 * A pruned node normally keeps only its most recent blocks, so the more nodes
 * prune, the fewer peers can serve the full history to a node syncing from
 * scratch. With sharding, a pruned node also keeps some "segments" of the
 * historical chain (fixed-size height ranges) and advertises the ones it holds
 * with the NODE_SHARD service bit and a "shardinfo" message. A syncing node can
 * then fetch each segment from any peer that holds it, so a set of sharded
 * nodes can together serve the whole chain.
 *
 * Segments are chosen with -shard=<percent> (a random but stable selection,
 * seeded from the "shardseed" file in the data directory) and/or explicitly
 * with -shardsegment=<n>. Block files that hold blocks of a selected segment
 * are never pruned; the -prune target applies to the remaining files.
 *
 * This only affects block storage and the P2P layer, not validation.
 */

/** Blocks per segment: about 17 days of LBRY blocks. Regtest can override it with -shardsegmentsize. */
static const uint32_t DEFAULT_SHARD_SEGMENT_SIZE = 10000;
/** Maximum number of segments in one shardinfo message. */
static const unsigned int MAX_SHARDINFO_SEGMENTS = 100000;

/** Read the sharding options. Must run after pruning is configured and the data directory is locked. */
bool InitSharding(std::string& error);
/** Whether this node keeps and advertises shard segments. */
bool ShardingEnabled();
/** Number of blocks per segment on this node. */
uint32_t ShardSegmentSize();
/** Whether this node has chosen to keep the given segment. */
bool IsShardSegmentSelected(uint32_t segment);
/** Whether a block file holds blocks of a selected segment, and so must not be pruned. */
bool ShardKeepsBlockFile(const CBlockFileInfo& info);

/**
 * The selected segments for which we have every block of the active chain, from
 * the start of the segment up to its end or to our tip, whichever comes first.
 * These are the segments we advertise and serve. Requires cs_main.
 */
std::vector<uint32_t> GetHeldShardSegments(const CChain& chain);

/** The configured selection, for RPC. */
struct ShardConfig {
    uint32_t segment_size;
    int percent;
    std::vector<uint32_t> segments;
};
ShardConfig GetShardConfig();

#endif // BITCOIN_SHARD_H
