// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include <functional>
#include <miner.h>
#include <primitives/transaction.h>
#include <test/claimtriefixture.h>
#include <validation.h>
#include <util/system.h>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(claimtriefixture_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(claimtriefixture_noop)
{
    BOOST_REQUIRE(true);
}

BOOST_AUTO_TEST_SUITE_END()

CMutableTransaction BuildTransaction(const CMutableTransaction& prev, uint32_t prevout, unsigned int numOutputs, int locktime)
{
    CMutableTransaction tx;
    tx.nVersion = CTransaction::CURRENT_VERSION;
    tx.vin.resize(1);
    tx.vout.resize(numOutputs);
    tx.vin[0].prevout.hash = prev.GetHash();
    tx.vin[0].prevout.n = prevout;
    tx.vin[0].scriptSig = CScript();
    if (locktime != 0) {
        // Use a relative locktime for validity X blocks in the future
        tx.nLockTime = ::ChainActive().Height() + locktime;
        tx.vin[0].nSequence = 0xffffffff - 1;
    } else {
        tx.nLockTime = 1 << 31; // Disable BIP68
        tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    }
    CAmount valuePerOutput = prev.vout[prevout].nValue;
    unsigned int numOutputsCopy = numOutputs;
    while ((numOutputsCopy = numOutputsCopy >> 1) > 0)
        valuePerOutput = valuePerOutput >> 1;

    for (unsigned int i = 0; i < numOutputs; ++i) {
        tx.vout[i].scriptPubKey = CScript();
        tx.vout[i].nValue = valuePerOutput;
    }
    return tx;
}

CMutableTransaction BuildTransaction(const uint256& prevhash)
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = prevhash;
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = CScript();
    tx.vout[0].nValue = 0;

    return tx;
}

BlockAssembler AssemblerForTest()
{
    BlockAssembler::Options options;
    options.nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
    options.blockMinFeeRate = CFeeRate(0);
    return BlockAssembler(Params(), options);
}

ClaimTrieChainFixture::ClaimTrieChainFixture() : CClaimTrieCache(&::Claimtrie()),
    unique_block_counter(0), normalization_original(-1), expirationForkHeight(-1), forkhash_original(-1),
    minRemovalWorkaroundHeight(-1), maxRemovalWorkaroundHeight(-1)
{
    fRequireStandard = false;
    BOOST_CHECK_EQUAL(nNextHeight, ::ChainActive().Height() + 1);
    setNormalizationForkHeight(1000000);

    gArgs.ForceSetArg("-limitancestorcount", "1000000");
    gArgs.ForceSetArg("-limitancestorsize", "1000000");
    gArgs.ForceSetArg("-limitdescendantcount", "1000000");
    gArgs.ForceSetArg("-limitdescendantsize", "1000000");

    num_txs_for_next_block = 0;
    coinbase_txs_used = 0;
    unique_block_counter = 0;
    added_unchecked = false;
    // generate coinbases to spend
    CreateCoinbases(40, coinbase_txs);
}

ClaimTrieChainFixture::~ClaimTrieChainFixture()
{
    added_unchecked = false;
    DecrementBlocks(::ChainActive().Height());
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (normalization_original >= 0) {
        consensus.nNormalizedNameForkHeight = normalization_original;
        const_cast<int&>(base->nNormalizedNameForkHeight) = normalization_original;
    }
    if (expirationForkHeight >= 0) {
        consensus.nExtendedClaimExpirationForkHeight = expirationForkHeight;
        consensus.nExtendedClaimExpirationTime = extendedExpiration;
        consensus.nOriginalClaimExpirationTime = originalExpiration;
        const_cast<int64_t&>(base->nExtendedClaimExpirationForkHeight) = expirationForkHeight;
        const_cast<int64_t&>(base->nOriginalClaimExpirationTime) = originalExpiration;
        const_cast<int64_t&>(base->nExtendedClaimExpirationTime) = extendedExpiration;
    }
    if (forkhash_original >= 0) {
        consensus.nAllClaimsInMerkleForkHeight = forkhash_original;
        const_cast<int64_t&>(base->nAllClaimsInMerkleForkHeight) = forkhash_original;
    }
    if (minRemovalWorkaroundHeight >= 0) {
        consensus.nMinRemovalWorkaroundHeight = minRemovalWorkaroundHeight;
        consensus.nMaxRemovalWorkaroundHeight = maxRemovalWorkaroundHeight;
        const_cast<int&>(base->nMinRemovalWorkaroundHeight) = minRemovalWorkaroundHeight;
        const_cast<int&>(base->nMaxRemovalWorkaroundHeight) = maxRemovalWorkaroundHeight;
    }
}

void ClaimTrieChainFixture::setRemovalWorkaroundHeight(int targetMinusCurrent, int blocks = 1000) {
    int target = ::ChainActive().Height() + targetMinusCurrent;
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (minRemovalWorkaroundHeight < 0) {
        minRemovalWorkaroundHeight = consensus.nMinRemovalWorkaroundHeight;
        maxRemovalWorkaroundHeight = consensus.nMaxRemovalWorkaroundHeight;
    }
    consensus.nMinRemovalWorkaroundHeight = target;
    consensus.nMaxRemovalWorkaroundHeight = target + blocks;
    const_cast<int&>(base->nMinRemovalWorkaroundHeight) = target;
    const_cast<int&>(base->nMaxRemovalWorkaroundHeight) = target + blocks;
}

void ClaimTrieChainFixture::setExpirationForkHeight(int targetMinusCurrent, int64_t preForkExpirationTime, int64_t postForkExpirationTime)
{
    int target = ::ChainActive().Height() + targetMinusCurrent;
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (expirationForkHeight < 0) {
        expirationForkHeight = consensus.nExtendedClaimExpirationForkHeight;
        originalExpiration = consensus.nOriginalClaimExpirationTime;
        extendedExpiration = consensus.nExtendedClaimExpirationTime;
    }
    consensus.nExtendedClaimExpirationForkHeight = target;
    consensus.nExtendedClaimExpirationTime = postForkExpirationTime;
    consensus.nOriginalClaimExpirationTime = preForkExpirationTime;
    const_cast<int64_t&>(base->nExtendedClaimExpirationForkHeight) = target;
    const_cast<int64_t&>(base->nOriginalClaimExpirationTime) = preForkExpirationTime;
    const_cast<int64_t&>(base->nExtendedClaimExpirationTime) = postForkExpirationTime;
}

void ClaimTrieChainFixture::setNormalizationForkHeight(int targetMinusCurrent)
{
    int target = ::ChainActive().Height() + targetMinusCurrent;
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (normalization_original < 0)
        normalization_original = consensus.nNormalizedNameForkHeight;
    consensus.nNormalizedNameForkHeight = target;
    const_cast<int&>(base->nNormalizedNameForkHeight) = target;
}

void ClaimTrieChainFixture::setHashForkHeight(int targetMinusCurrent)
{
    int target = ::ChainActive().Height() + targetMinusCurrent;
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (forkhash_original < 0)
        forkhash_original = consensus.nAllClaimsInMerkleForkHeight;
    consensus.nAllClaimsInMerkleForkHeight = target;
    const_cast<int64_t&>(base->nAllClaimsInMerkleForkHeight) = target;
}

bool ClaimTrieChainFixture::CreateBlock(const std::unique_ptr<CBlockTemplate>& pblocktemplate)
{
    CBlock* pblock = &pblocktemplate->block;
    {
        LOCK(cs_main);
        pblock->nVersion = 5;
        pblock->hashPrevBlock = ::ChainActive().Tip()->GetBlockHash();
        pblock->nTime = ::ChainActive().Tip()->GetBlockTime() + Params().GetConsensus().nPowTargetSpacing;
        CMutableTransaction txCoinbase(*pblock->vtx[0]);
        txCoinbase.vin[0].scriptSig = CScript() << int(::ChainActive().Height() + 1) << int(++unique_block_counter);
        txCoinbase.vout[0].nValue = GetBlockSubsidy(::ChainActive().Height() + 1, Params().GetConsensus());
        pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
        pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        for (uint32_t i = 0;; ++i) {
            pblock->nNonce = i;
            if (CheckProofOfWork(pblock->GetPoWHash(), pblock->nBits, Params().GetConsensus()))
                break;
        }
    }
    auto success = ProcessNewBlock(Params(), std::make_shared<const CBlock>(*pblock), true, nullptr);
    return success && pblock->GetHash() == ::ChainActive().Tip()->GetBlockHash();
}

bool ClaimTrieChainFixture::CreateCoinbases(unsigned int num_coinbases, std::vector<CTransaction>& coinbases)
{
    std::unique_ptr<CBlockTemplate> pblocktemplate;
    coinbases.clear();
    BOOST_REQUIRE(pblocktemplate = AssemblerForTest().CreateNewBlock(CScript() << OP_TRUE));
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 1);
    for (unsigned int i = 0; i < 100 + num_coinbases; ++i) {
        if (!CreateBlock(pblocktemplate)) {
            BOOST_REQUIRE(pblocktemplate = AssemblerForTest().CreateNewBlock(CScript() << OP_TRUE));
            BOOST_REQUIRE(CreateBlock(pblocktemplate));
        }
        if (coinbases.size() < num_coinbases)
            coinbases.push_back(std::move(*pblocktemplate->block.vtx[0]));
    }
    return true;
}

void ClaimTrieChainFixture::CommitTx(const CMutableTransaction &tx, bool has_locktime)
{
    num_txs_for_next_block++;
    if (has_locktime) {
        added_unchecked = true;
        TestMemPoolEntryHelper entry;
        LOCK(mempool.cs);
        mempool.addUnchecked(entry.Fee(0).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
    } else {
        CValidationState state;
        CAmount txFeeRate = CAmount(0);
        LOCK(cs_main);
        BOOST_REQUIRE(AcceptToMemoryPool(mempool, state, MakeTransactionRef(tx), nullptr, nullptr, false, txFeeRate, false));
    }
}

// spend a bid into some non claimtrie related unspent
CMutableTransaction ClaimTrieChainFixture::Spend(const CMutableTransaction &prev)
{
    CMutableTransaction tx = BuildTransaction(prev, 0);
    tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    tx.vout[0].nValue = prev.vout[0].nValue;

    CommitTx(tx);
    return tx;
}

// make claim at the current block
CMutableTransaction ClaimTrieChainFixture::MakeClaim(const CMutableTransaction& prev, const std::string& name, const std::string& value, CAmount quantity, int locktime)
{
    uint32_t prevout = prev.vout.size() - 1;
    while (prevout > 0 && prev.vout[prevout].nValue < quantity)
        --prevout;
    CMutableTransaction tx = BuildTransaction(prev, prevout, prev.vout[prevout].nValue > quantity ? 2 : 1, locktime);
    tx.vout[0].scriptPubKey = ClaimNameScript(name, value);
    tx.vout[0].nValue = quantity;
    if (tx.vout.size() > 1) {
        tx.vout[1].scriptPubKey = CScript() << OP_TRUE;
        tx.vout[1].nValue = prev.vout[prevout].nValue - quantity;
    }

    CommitTx(tx, locktime != 0);
    return tx;
}

CMutableTransaction ClaimTrieChainFixture::MakeClaim(const CMutableTransaction& prev, const std::string& name, const std::string& value)
{
    return MakeClaim(prev, name, value, prev.vout[0].nValue, 0);
}

// make support at the current block
CMutableTransaction ClaimTrieChainFixture::MakeSupport(const CMutableTransaction &prev, const CMutableTransaction &claimtx, const std::string& name, CAmount quantity)
{
    uint32_t prevout = prev.vout.size() - 1;
    while (prevout > 0 && prev.vout[prevout].nValue < quantity)
        --prevout;

    CMutableTransaction tx = BuildTransaction(prev, prevout, prev.vout[prevout].nValue > quantity ? 2 : 1);
    tx.vout[0].scriptPubKey = SupportClaimScript(name, ClaimIdHash(claimtx.GetHash(), 0));
    tx.vout[0].nValue = quantity;
    if (tx.vout.size() > 1) {
        tx.vout[1].scriptPubKey = CScript() << OP_TRUE;
        tx.vout[1].nValue = prev.vout[prevout].nValue - quantity;
    }

    CommitTx(tx);
    return tx;
}

// make update at the current block
CMutableTransaction ClaimTrieChainFixture::MakeUpdate(const CMutableTransaction &prev, const std::string& name, const std::string& value, const uint160& claimId, CAmount quantity)
{
    CMutableTransaction tx = BuildTransaction(prev, 0);
    if (prev.vout[0].nValue < quantity) {
        auto coverIt = GetCoinbase();
        tx.vin.push_back(CTxIn(coverIt.GetHash(), 0));
    }

    tx.vout[0].scriptPubKey = UpdateClaimScript(name, claimId, value);
    tx.vout[0].nValue = quantity;

    CommitTx(tx);
    return tx;
}

CMutableTransaction ClaimTrieChainFixture::GetCoinbase()
{
    return CMutableTransaction(coinbase_txs.at(coinbase_txs_used++));
}

// create i blocks
void ClaimTrieChainFixture::IncrementBlocks(int num_blocks, bool mark)
{
    if (mark)
        marks.push_back(::ChainActive().Height());

    for (int i = 0; i < num_blocks; ++i) {
        CScript coinbase_scriptpubkey;
        coinbase_scriptpubkey << CScriptNum(::ChainActive().Height());
        std::unique_ptr<CBlockTemplate> pblocktemplate = AssemblerForTest().CreateNewBlock(coinbase_scriptpubkey);
        BOOST_REQUIRE(pblocktemplate != nullptr);
        if (!added_unchecked)
            BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), num_txs_for_next_block + 1);
        BOOST_REQUIRE(CreateBlock(pblocktemplate));
        num_txs_for_next_block = 0;
        expirationHeight = ::ChainActive().Height();
        nNextHeight = ::ChainActive().Height() + 1;
    }
}

// disconnect i blocks from tip
void ClaimTrieChainFixture::DecrementBlocks(int num_blocks)
{
    CValidationState state;
    {
        LOCK(cs_main);
        CBlockIndex* pblockindex = ::ChainActive()[::ChainActive().Height() - num_blocks + 1];
        BOOST_REQUIRE(InvalidateBlock(state, Params(), pblockindex));
    }
    BOOST_REQUIRE(state.IsValid());
    BOOST_REQUIRE(ActivateBestChain(state, Params()));
    mempool.clear();
    num_txs_for_next_block = 0;
    expirationHeight = ::ChainActive().Height();
    nNextHeight = ::ChainActive().Height() + 1;
}

// decrement back to last mark
void ClaimTrieChainFixture::DecrementBlocks()
{
    int mark = marks.back();
    marks.pop_back();
    DecrementBlocks(::ChainActive().Height() - mark);
}

bool ClaimTrieChainFixture::queueEmpty() const
{
    int64_t count;
    db << "SELECT COUNT(*) FROM claim WHERE validHeight >= ?" << nNextHeight >> count;
    return count == 0;
}

bool ClaimTrieChainFixture::expirationQueueEmpty() const
{
    int64_t count;
    db << "SELECT COUNT(*) FROM claim WHERE expirationHeight >= ?" << nNextHeight >> count;
    return count == 0;
}

bool ClaimTrieChainFixture::supportEmpty() const
{
    int64_t count;
    db << "SELECT COUNT(*) FROM support WHERE validHeight < ? AND expirationHeight >= ?" << nNextHeight << nNextHeight >> count;
    return count == 0;
}

bool ClaimTrieChainFixture::supportQueueEmpty() const
{
    int64_t count;
    db << "SELECT COUNT(*) FROM support WHERE validHeight >= ?" << nNextHeight >> count;
    return count == 0;
}

int ClaimTrieChainFixture::proportionalDelayFactor() const
{
    return base->nProportionalDelayFactor;
}

boost::test_tools::predicate_result negativeResult(const std::function<void(boost::wrap_stringstream&)>& callback)
{
    boost::test_tools::predicate_result res(false);
    callback(res.message());
    return res;
}

boost::test_tools::predicate_result negativeResult(const std::string& message)
{
    return negativeResult([&message](boost::wrap_stringstream& stream) {
        stream << message;
    });
}

// is a claim in queue
boost::test_tools::predicate_result ClaimTrieChainFixture::is_claim_in_queue(const std::string& name, const CMutableTransaction &tx)
{
    COutPoint outPoint(tx.GetHash(), 0);
    int validAtHeight;
    if (haveClaimInQueue(name, outPoint, validAtHeight))
        return true;
    return negativeResult("Is not a claim in queue");
}

// check if tx is best claim based on outpoint
boost::test_tools::predicate_result ClaimTrieChainFixture::is_best_claim(const std::string& name, const CMutableTransaction &tx)
{
    CClaimValue val;
    COutPoint outPoint(tx.GetHash(), 0);
    bool have_claim = haveClaim(name, outPoint);
    bool have_info = getInfoForName(name, val);
    if (have_claim && have_info && val.outPoint == outPoint)
        return true;
    return negativeResult("Is not best claim");
}

// check effective quantity of best claim
boost::test_tools::predicate_result ClaimTrieChainFixture::best_claim_effective_amount_equals(const std::string& name, CAmount amount)
{
    CClaimValue val;
    bool have_info = getInfoForName(name, val);
    if (!have_info)
        return negativeResult("No claim found");
    CAmount effective_amount = getClaimsForName(name).find(val.claimId).effectiveAmount;
    if (effective_amount != amount)
        return negativeResult([amount, effective_amount](boost::wrap_stringstream& stream) {
            stream << amount << " != " << effective_amount;
        });
    return true;
}

bool ClaimTrieChainFixture::getClaimById(const uint160 &claimId, std::string &name, CClaimValue &value)
{
    auto query = db << "SELECT nodeName, claimID, txID, txN, amount, validHeight, blockHeight "
                       "FROM claim WHERE claimID = ?" << claimId;
    auto hit = false;
    for (auto&& row: query) {
        if (hit) return false;
        row >> name >> value.claimId >> value.outPoint.hash >> value.outPoint.n
            >> value.nAmount >> value.nValidAtHeight >> value.nHeight;
        hit = true;
    }
    return hit;
}

int64_t ClaimTrieChainFixture::nodeCount() const {
    int64_t ret = 0;
    db << "SELECT COUNT(*) FROM node" >> ret;
    return ret;
}

std::vector<std::string> ClaimTrieChainFixture::getNodeChildren(const std::string &name)
{
    std::vector<std::string> ret;
    for (auto&& row: db << "SELECT name FROM node WHERE parent = ?" << name) {
        ret.emplace_back();
        row >> ret.back();
    }
    return ret;
}
