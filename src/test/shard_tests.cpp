// Copyright (c) 2026 The LBRY developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <fs.h>
#include <shard.h>
#include <test/setup_common.h>
#include <util/system.h>
#include <validation.h>

#include <algorithm>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace {

/** Regtest setup (so -shardsegmentsize is allowed) with pruning on; resets sharding afterwards. */
struct ShardTestingSetup : public BasicTestingSetup {
    ShardTestingSetup() : BasicTestingSetup(CBaseChainParams::REGTEST)
    {
        fPruneMode = true;
    }
    ~ShardTestingSetup()
    {
        gArgs.ForceSetArg("-shard", "0");
        gArgs.ForceSetArg("-shardsegmentsize", strprintf("%u", DEFAULT_SHARD_SEGMENT_SIZE));
        fPruneMode = false;
        std::string error;
        InitSharding(error);
    }

    bool Init(int percent, std::string* error_out = nullptr)
    {
        gArgs.ForceSetArg("-shard", strprintf("%d", percent));
        std::string error;
        const bool ok = InitSharding(error);
        if (error_out) *error_out = error;
        return ok;
    }

    std::vector<bool> Selection(uint32_t count)
    {
        std::vector<bool> selected;
        for (uint32_t segment = 0; segment < count; ++segment) {
            selected.push_back(IsShardSegmentSelected(segment));
        }
        return selected;
    }
};

CBlockFileInfo FileWithHeights(unsigned int first, unsigned int last)
{
    CBlockFileInfo info;
    for (unsigned int height = first; height <= last; ++height) {
        info.AddBlock(height, 0);
    }
    return info;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(shard_tests, ShardTestingSetup)

BOOST_AUTO_TEST_CASE(disabled_by_default)
{
    BOOST_CHECK(Init(0));
    BOOST_CHECK(!ShardingEnabled());
    BOOST_CHECK(!IsShardSegmentSelected(0));
    BOOST_CHECK(!ShardKeepsBlockFile(FileWithHeights(0, 100)));
}

BOOST_AUTO_TEST_CASE(invalid_options)
{
    std::string error;
    fPruneMode = false;
    BOOST_CHECK(!Init(10, &error));
    BOOST_CHECK_EQUAL(error, "Sharding (-shard or -shardsegment) requires pruning (-prune).");
    BOOST_CHECK(!ShardingEnabled());

    fPruneMode = true;
    BOOST_CHECK(!Init(101, &error));
    BOOST_CHECK(!Init(-1, &error));
    BOOST_CHECK(!ShardingEnabled());
}

BOOST_AUTO_TEST_CASE(percent_selection_is_random_and_stable)
{
    const fs::path seed_path = GetDataDir() / "shardseed";
    fs::remove(seed_path);

    BOOST_CHECK(Init(25));
    BOOST_CHECK(ShardingEnabled());
    BOOST_CHECK(fs::exists(seed_path));
    const std::vector<bool> first = Selection(10000);
    const int selected = std::count(first.begin(), first.end(), true);
    // Expect 2500 with a standard deviation of about 43.
    BOOST_CHECK_GT(selected, 2200);
    BOOST_CHECK_LT(selected, 2800);

    // The seed file keeps the selection across restarts...
    BOOST_CHECK(Init(25));
    BOOST_CHECK(Selection(10000) == first);

    // ...and a new seed gives a different one.
    fs::remove(seed_path);
    BOOST_CHECK(Init(25));
    BOOST_CHECK(Selection(10000) != first);

    BOOST_CHECK(Init(100));
    const std::vector<bool> all = Selection(1000);
    BOOST_CHECK(std::count(all.begin(), all.end(), true) == 1000);
}

BOOST_AUTO_TEST_CASE(keeps_files_overlapping_selected_segments)
{
    gArgs.ForceSetArg("-shardsegmentsize", "100");
    BOOST_CHECK(Init(50));
    BOOST_CHECK_EQUAL(ShardSegmentSize(), 100U);

    // Find a selected segment followed by an unselected one.
    uint32_t kept = 0;
    while (!(IsShardSegmentSelected(kept) && !IsShardSegmentSelected(kept + 1))) ++kept;
    const unsigned int start = kept * 100;

    BOOST_CHECK(ShardKeepsBlockFile(FileWithHeights(start, start + 99)));
    // A file straddling the boundary holds blocks of the selected segment.
    BOOST_CHECK(ShardKeepsBlockFile(FileWithHeights(start + 90, start + 120)));
    // A file only holding blocks of the unselected segment can be pruned.
    BOOST_CHECK(!ShardKeepsBlockFile(FileWithHeights(start + 100, start + 199)));
    // So can a file with no blocks.
    BOOST_CHECK(!ShardKeepsBlockFile(CBlockFileInfo()));
}

BOOST_AUTO_TEST_CASE(held_segments)
{
    gArgs.ForceSetArg("-shardsegmentsize", "10");
    BOOST_CHECK(Init(100));

    // A chain of 35 blocks: segments 0-2 are complete, segment 3 holds heights 30-34 so far.
    std::vector<CBlockIndex> blocks(35);
    for (size_t height = 0; height < blocks.size(); ++height) {
        blocks[height].nHeight = height;
        blocks[height].pprev = height > 0 ? &blocks[height - 1] : nullptr;
        blocks[height].nStatus = BLOCK_HAVE_DATA;
    }
    CChain chain;
    chain.SetTip(&blocks.back());

    LOCK(cs_main);
    BOOST_CHECK(GetHeldShardSegments(chain) == std::vector<uint32_t>({0, 1, 2, 3}));

    // A missing block (for example pruned before sharding was enabled) means the segment is not held.
    blocks[15].nStatus &= ~BLOCK_HAVE_DATA;
    BOOST_CHECK(GetHeldShardSegments(chain) == std::vector<uint32_t>({0, 2, 3}));
}

BOOST_AUTO_TEST_SUITE_END()
