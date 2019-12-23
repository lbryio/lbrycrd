// Copyright (c) 2016-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <claimtrie/hashes.h>
#include <consensus/merkle.h>
#include <crypto/sha256.h>
#include <random.h>
#include <uint256.h>

static void MerkleRootUni(benchmark::State& state)
{
    FastRandomContext rng(true);
    std::vector<uint256> leaves;
    leaves.resize(9001);
    for (auto& item : leaves) {
        item = rng.rand256();
    }
    while (state.KeepRunning()) {
        bool mutation = false;
        uint256 hash = ComputeMerkleRoot(leaves, &mutation);
        leaves[mutation] = hash;
    }
}

static void MerkleRoot(benchmark::State& state)
{
    sha256n_way = [](std::vector<uint256>& hashes) {
        SHA256D64(hashes[0].begin(), hashes[0].begin(), hashes.size() / 2);
    };
    MerkleRootUni(state);
}

BENCHMARK(MerkleRoot, 800);
BENCHMARK(MerkleRootUni, 800);
