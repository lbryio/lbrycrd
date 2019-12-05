// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#ifndef _CLAIMTRIEFIXTURE_H_
#define _CLAIMTRIEFIXTURE_H_

#include <chainparams.h>
#include <claimtrie/forks.h>
#include <coins.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <miner.h>
#include <nameclaim.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <random.h>
#include <rpc/claimrpchelp.h>
#include <rpc/server.h>
#include <streams.h>
#include <test/test_bitcoin.h>
#include <txmempool.h>
#include <util.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>
#include <iostream>

extern ::CChainState g_chainstate;
extern ::ArgsManager gArgs;
extern std::vector<std::string> random_strings(std::size_t count);


CMutableTransaction BuildTransaction(const uint256& prevhash);
CMutableTransaction BuildTransaction(const CTransaction& prev, uint32_t prevout=0, unsigned int numOutputs=1, int locktime=0);

BlockAssembler AssemblerForTest();

// Test Fixtures
struct ClaimTrieChainFixture: public CClaimTrieCache
{
    std::vector<CTransaction> coinbase_txs;
    std::vector<int> marks;
    int coinbase_txs_used;
    int unique_block_counter;
    int normalization_original;
    unsigned int num_txs_for_next_block;
    bool added_unchecked;

    int64_t expirationForkHeight;
    int64_t originalExpiration;
    int64_t extendedExpiration;
    int64_t forkhash_original;

    using CClaimTrieCache::getSupportsForName;

    ClaimTrieChainFixture();

    ~ClaimTrieChainFixture() override;

    void setExpirationForkHeight(int targetMinusCurrent, int64_t preForkExpirationTime, int64_t postForkExpirationTime);

    void setNormalizationForkHeight(int targetMinusCurrent);

    void setHashForkHeight(int targetMinusCurrent);

    bool CreateBlock(const std::unique_ptr<CBlockTemplate>& pblocktemplate);

    bool CreateCoinbases(unsigned int num_coinbases, std::vector<CTransaction>& coinbases);

    void CommitTx(const CMutableTransaction &tx, bool has_locktime=false);

    // spend a bid into some non claimtrie related unspent
    CMutableTransaction Spend(const CTransaction &prev);

    // make claim at the current block
    CMutableTransaction MakeClaim(const CTransaction& prev, const std::string& name, const std::string& value, CAmount quantity, int locktime=0);

    CMutableTransaction MakeClaim(const CTransaction& prev, const std::string& name, const std::string& value);

    // make support at the current block
    CMutableTransaction MakeSupport(const CTransaction &prev, const CTransaction &claimtx, const std::string& name, CAmount quantity);

    // make update at the current block
    CMutableTransaction MakeUpdate(const CTransaction &prev, const std::string& name, const std::string& value, const uint160& claimId, CAmount quantity);

    CTransaction GetCoinbase();

    // create i blocks
    void IncrementBlocks(int num_blocks, bool mark = false);

    // disconnect i blocks from tip
    void DecrementBlocks(int num_blocks);

    // decrement back to last mark
    void DecrementBlocks();

    bool queueEmpty() const;

    bool expirationQueueEmpty() const;

    bool supportEmpty() const;

    bool supportQueueEmpty() const;

    int proportionalDelayFactor() const;

    bool getClaimById(const CUint160& claimId, std::string& name, CClaimValue& value);

    // is a claim in queue
    boost::test_tools::predicate_result is_claim_in_queue(const std::string& name, const CTransaction &tx);

    // check if tx is best claim based on outpoint
    boost::test_tools::predicate_result is_best_claim(const std::string& name, const CTransaction &tx);

    // check effective quantity of best claim
    boost::test_tools::predicate_result best_claim_effective_amount_equals(const std::string& name, CAmount amount);

    std::vector<std::string> getNodeChildren(const std::string& name);
};

#endif // _CLAIMTRIEFIXTURE_H_
