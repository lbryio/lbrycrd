// Copyright (c) 2016-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>

#include <chainparams.h>
#include <validation.h>
#include <streams.h>
#include <consensus/validation.h>


/* Don't use raw bitcoin blocks
*/

CDataStream getTestBlockStream()
{
    static CBlock block;
    static CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    if (block.IsNull()) {
        block.nVersion = 5;
        block.hashPrevBlock = uint256S("e8fc54d1e6581fceaaf64cc8afeedb9c4ba1eb2349eaf638f1fc41e331869dee");
        block.hashMerkleRoot = uint256S("a926580973e1204aa179a4c536023c69212ce00b56dc85d3516488f3c51dd022");
        block.hashClaimTrie = uint256S("7bae80d60b09031265f8a9f6282e4b9653764fadfac2c8b4c1f416c972b58814");
        block.nTime = 1545050539;
        block.nBits = 0x207fffff;
        block.nNonce = 128913;
        block.vtx.resize(60);
        uint256 prevHash;
        for (int i = 0; i < 60; ++i) {
            CMutableTransaction tx;
            tx.nVersion = 5;
            tx.nLockTime = 0;
            tx.vin.resize(1);
            tx.vout.resize(1);
            if (!prevHash.IsNull()) {
                tx.vin[0].prevout.hash = prevHash;
                tx.vin[0].prevout.n = 0;
            }
            tx.vin[0].scriptSig = CScript() << OP_0 << OP_0;
            tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
            tx.vout[0].scriptPubKey = CScript();
            tx.vout[0].nValue = 0;
            prevHash = tx.GetHash();
            block.vtx[i] = MakeTransactionRef(std::move(tx));
        }
        stream << block;
        char a = '\0';
        stream.write(&a, 1); // Prevent compaction
    }
    return stream;
}
// These are the two major time-sinks which happen after we have fully received
// a block off the wire, but before we can relay the block on to peers using
// compact block relay.

static void DeserializeBlockTest(benchmark::State& state)
{
    auto stream = getTestBlockStream();
    auto size = stream.size() - 1;
    while (state.KeepRunning()) {
        CBlock block;
        stream >> block;
        assert(stream.Rewind(size));
    }
}

static void DeserializeAndCheckBlockTest(benchmark::State& state)
{
    auto stream = getTestBlockStream();
    auto size = stream.size() - 1;
    const auto chainParams = CreateChainParams(CBaseChainParams::REGTEST);

    while (state.KeepRunning()) {
        CBlock block; // Note that CBlock caches its checked state, so we need to recreate it here
        stream >> block;
        assert(stream.Rewind(size));

        CValidationState validationState;
        bool checked = CheckBlock(block, validationState, chainParams->GetConsensus());
        assert(checked);
    }
}

BENCHMARK(DeserializeBlockTest, 130);
BENCHMARK(DeserializeAndCheckBlockTest, 160);
