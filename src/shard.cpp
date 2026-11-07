// Copyright (c) 2026 The LBRY developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <shard.h>

#include <chain.h>
#include <chainparams.h>
#include <crypto/siphash.h>
#include <fs.h>
#include <logging.h>
#include <random.h>
#include <tinyformat.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>

#include <algorithm>
#include <limits>
#include <set>

namespace {

// Set once by InitSharding() before any other thread starts, read-only afterwards.
bool g_enabled = false;
uint32_t g_segment_size = DEFAULT_SHARD_SEGMENT_SIZE;
int g_percent = 0;
std::set<uint32_t> g_segments;
uint256 g_seed;

const char* const SHARD_SEED_FILENAME = "shardseed";

/**
 * Load the random seed behind -shard's selection, creating it on first use.
 * Keeping it on disk keeps the selection stable across restarts: blocks of a
 * segment dropped from the selection are pruned and can't be fetched again.
 */
bool LoadOrCreateSeed(uint256& seed, std::string& error)
{
    const fs::path path = GetDataDir() / SHARD_SEED_FILENAME;
    fsbridge::ifstream in(path);
    if (in.is_open()) {
        std::string hex;
        in >> hex;
        if (hex.size() != 64 || !IsHex(hex)) {
            error = strprintf("Invalid shard seed in %s", path.string());
            return false;
        }
        seed.SetHex(hex);
        return true;
    }

    seed = GetRandHash();
    fsbridge::ofstream out(path);
    out << seed.GetHex() << "\n";
    out.close();
    if (out.fail()) {
        error = strprintf("Unable to write shard seed to %s", path.string());
        return false;
    }
    LogPrintf("Created new shard seed in %s\n", path.string());
    return true;
}

} // namespace

bool InitSharding(std::string& error)
{
    g_enabled = false;
    g_segment_size = DEFAULT_SHARD_SEGMENT_SIZE;
    g_percent = 0;
    g_segments.clear();
    g_seed.SetNull();

    const int64_t percent = gArgs.GetArg("-shard", 0);
    const std::vector<std::string> segment_args = gArgs.GetArgs("-shardsegment");
    if (percent == 0 && segment_args.empty()) {
        return true;
    }
    if (!fPruneMode) {
        error = "Sharding (-shard or -shardsegment) requires pruning (-prune).";
        return false;
    }
    if (percent < 0 || percent > 100) {
        error = strprintf("-shard must be a percentage from 0 to 100 (got %d).", percent);
        return false;
    }
    for (const std::string& arg : segment_args) {
        int32_t segment;
        if (!ParseInt32(arg, &segment) || segment < 0) {
            error = strprintf("Invalid -shardsegment '%s': expected a segment number (0 or more).", arg);
            return false;
        }
        g_segments.insert(segment);
    }
    if (gArgs.IsArgSet("-shardsegmentsize")) {
        if (Params().NetworkIDString() != CBaseChainParams::REGTEST) {
            error = "-shardsegmentsize is only supported on regtest.";
            return false;
        }
        const int64_t size = gArgs.GetArg("-shardsegmentsize", DEFAULT_SHARD_SEGMENT_SIZE);
        if (size < 1 || size > std::numeric_limits<int32_t>::max()) {
            error = strprintf("Invalid -shardsegmentsize %d.", size);
            return false;
        }
        g_segment_size = size;
    }
    if (percent > 0 && !LoadOrCreateSeed(g_seed, error)) {
        return false;
    }
    g_percent = percent;
    g_enabled = true;

    std::string explicit_segments;
    for (uint32_t segment : g_segments) {
        explicit_segments += strprintf(" %u", segment);
    }
    LogPrintf("Sharding enabled: segment size %u blocks, keeping %d%% of segments at random%s%s\n",
        g_segment_size, g_percent, g_segments.empty() ? "" : " plus segments", explicit_segments);
    return true;
}

bool ShardingEnabled()
{
    return g_enabled;
}

uint32_t ShardSegmentSize()
{
    return g_segment_size;
}

bool IsShardSegmentSelected(uint32_t segment)
{
    if (!g_enabled) return false;
    if (g_segments.count(segment)) return true;
    if (g_percent <= 0) return false;
    if (g_percent >= 100) return true;
    const uint64_t hash = CSipHasher(g_seed.GetUint64(0), g_seed.GetUint64(1)).Write(segment).Finalize();
    return hash % 100 < (uint64_t)g_percent;
}

bool ShardKeepsBlockFile(const CBlockFileInfo& info)
{
    if (!g_enabled || info.nBlocks == 0) return false;
    const uint32_t first = info.nHeightFirst / g_segment_size;
    const uint32_t last = info.nHeightLast / g_segment_size;
    for (uint32_t segment = first; segment <= last; ++segment) {
        if (IsShardSegmentSelected(segment)) return true;
    }
    return false;
}

std::vector<uint32_t> GetHeldShardSegments(const CChain& chain)
{
    AssertLockHeld(cs_main);
    std::vector<uint32_t> held;
    const int tip_height = chain.Height();
    if (!g_enabled || tip_height < 0) return held;

    const uint32_t tip_segment = tip_height / g_segment_size;
    for (uint32_t segment = 0; segment <= tip_segment && held.size() < MAX_SHARDINFO_SEGMENTS; ++segment) {
        if (!IsShardSegmentSelected(segment)) continue;
        const int first = (int64_t)segment * g_segment_size;
        const int last = std::min<int64_t>((int64_t)first + g_segment_size - 1, tip_height);
        bool have_all = true;
        for (int height = first; height <= last; ++height) {
            if (!(chain[height]->nStatus & BLOCK_HAVE_DATA)) {
                have_all = false;
                break;
            }
        }
        if (have_all) held.push_back(segment);
    }
    return held;
}

ShardConfig GetShardConfig()
{
    return ShardConfig{g_segment_size, g_percent, std::vector<uint32_t>(g_segments.begin(), g_segments.end())};
}
