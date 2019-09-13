// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include <functional>
#include <test/claimtriefixture.h>

using namespace std;

CMutableTransaction BuildTransaction(const CTransaction& prev, uint32_t prevout, unsigned int numOutputs, int locktime)
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
        tx.nLockTime = chainActive.Height() + locktime;
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

ClaimTrieChainFixture::ClaimTrieChainFixture() : CClaimTrieCache(pclaimTrie),
    unique_block_counter(0), normalization_original(-1), expirationForkHeight(-1), forkhash_original(-1)
{
    fRequireStandard = false;
    BOOST_CHECK_EQUAL(nNextHeight, chainActive.Height() + 1);
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
    DecrementBlocks(chainActive.Height());
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (normalization_original >= 0)
        consensus.nNormalizedNameForkHeight = normalization_original;

    if (expirationForkHeight >= 0) {
        consensus.nExtendedClaimExpirationForkHeight = expirationForkHeight;
        consensus.nExtendedClaimExpirationTime = extendedExpiration;
        consensus.nOriginalClaimExpirationTime = originalExpiration;
    }
    if (forkhash_original >= 0)
        consensus.nAllClaimsInMerkleForkHeight = forkhash_original;
}

void ClaimTrieChainFixture::setExpirationForkHeight(int targetMinusCurrent, int64_t preForkExpirationTime, int64_t postForkExpirationTime)
{
    int target = chainActive.Height() + targetMinusCurrent;
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (expirationForkHeight < 0) {
        expirationForkHeight = consensus.nExtendedClaimExpirationForkHeight;
        originalExpiration = consensus.nOriginalClaimExpirationTime;
        extendedExpiration = consensus.nExtendedClaimExpirationTime;
    }
    consensus.nExtendedClaimExpirationForkHeight = target;
    consensus.nExtendedClaimExpirationTime = postForkExpirationTime;
    consensus.nOriginalClaimExpirationTime = preForkExpirationTime;
    setExpirationTime(targetMinusCurrent >= 0 ? preForkExpirationTime : postForkExpirationTime);
}

void ClaimTrieChainFixture::setNormalizationForkHeight(int targetMinusCurrent)
{
    int target = chainActive.Height() + targetMinusCurrent;
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (normalization_original < 0)
        normalization_original = consensus.nNormalizedNameForkHeight;
    consensus.nNormalizedNameForkHeight = target;
}

void ClaimTrieChainFixture::setHashForkHeight(int targetMinusCurrent)
{
    int target = chainActive.Height() + targetMinusCurrent;
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    if (forkhash_original < 0)
        forkhash_original = consensus.nAllClaimsInMerkleForkHeight;
    consensus.nAllClaimsInMerkleForkHeight = target;
}

bool ClaimTrieChainFixture::CreateBlock(const std::unique_ptr<CBlockTemplate>& pblocktemplate)
{
    CBlock* pblock = &pblocktemplate->block;
    {
        LOCK(cs_main);
        pblock->nVersion = 5;
        pblock->hashPrevBlock = chainActive.Tip()->GetBlockHash();
        pblock->nTime = chainActive.Tip()->GetBlockTime() + Params().GetConsensus().nPowTargetSpacing;
        CMutableTransaction txCoinbase(*pblock->vtx[0]);
        txCoinbase.vin[0].scriptSig = CScript() << int(chainActive.Height() + 1) << int(++unique_block_counter);
        txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
        pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
        pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        for (uint32_t i = 0;; ++i) {
            pblock->nNonce = i;
            if (CheckProofOfWork(pblock->GetPoWHash(), pblock->nBits, Params().GetConsensus()))
                break;
        }
    }
    auto success = ProcessNewBlock(Params(), std::make_shared<const CBlock>(*pblock), true, nullptr);
    return success && pblock->GetHash() == chainActive.Tip()->GetBlockHash();
}

bool ClaimTrieChainFixture::CreateCoinbases(unsigned int num_coinbases, std::vector<CTransaction>& coinbases)
{
    std::unique_ptr<CBlockTemplate> pblocktemplate;
    coinbases.clear();
    BOOST_CHECK(pblocktemplate = AssemblerForTest().CreateNewBlock(CScript() << OP_TRUE));
    BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), 1);
    for (unsigned int i = 0; i < 100 + num_coinbases; ++i) {
        BOOST_CHECK(CreateBlock(pblocktemplate));
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
        mempool.addUnchecked(tx.GetHash(), entry.Fee(0).Time(GetTime()).SpendsCoinbase(true).FromTx(tx));
    } else {
        CValidationState state;
        CAmount txFeeRate = CAmount(0);
        LOCK(cs_main);
        BOOST_CHECK_EQUAL(AcceptToMemoryPool(mempool, state, MakeTransactionRef(tx), nullptr, nullptr, false, txFeeRate, false), true);
    }
}

// spend a bid into some non claimtrie related unspent
CMutableTransaction ClaimTrieChainFixture::Spend(const CTransaction &prev)
{
    CMutableTransaction tx = BuildTransaction(prev, 0);
    tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    tx.vout[0].nValue = prev.vout[0].nValue;

    CommitTx(tx);
    return tx;
}

// make claim at the current block
CMutableTransaction ClaimTrieChainFixture::MakeClaim(const CTransaction& prev, const std::string& name, const std::string& value, CAmount quantity, int locktime)
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

CMutableTransaction ClaimTrieChainFixture::MakeClaim(const CTransaction& prev, const std::string& name, const std::string& value)
{
    return MakeClaim(prev, name, value, prev.vout[0].nValue, 0);
}

// make support at the current block
CMutableTransaction ClaimTrieChainFixture::MakeSupport(const CTransaction &prev, const CTransaction &claimtx, const std::string& name, CAmount quantity)
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
CMutableTransaction ClaimTrieChainFixture::MakeUpdate(const CTransaction &prev, const std::string& name, const std::string& value, const uint160& claimId, CAmount quantity)
{
    CMutableTransaction tx = BuildTransaction(prev, 0);
    tx.vout[0].scriptPubKey = UpdateClaimScript(name, claimId, value);
    tx.vout[0].nValue = quantity;

    CommitTx(tx);
    return tx;
}

CTransaction ClaimTrieChainFixture::GetCoinbase()
{
    return coinbase_txs.at(coinbase_txs_used++);
}

// create i blocks
void ClaimTrieChainFixture::IncrementBlocks(int num_blocks, bool mark)
{
    if (mark)
        marks.push_back(chainActive.Height());

    clear(); // clears the internal cache
    for (int i = 0; i < num_blocks; ++i) {
        CScript coinbase_scriptpubkey;
        coinbase_scriptpubkey << CScriptNum(chainActive.Height());
        std::unique_ptr<CBlockTemplate> pblocktemplate = AssemblerForTest().CreateNewBlock(coinbase_scriptpubkey);
        BOOST_CHECK(pblocktemplate != nullptr);
        if (!added_unchecked)
            BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), num_txs_for_next_block + 1);
        BOOST_CHECK_EQUAL(CreateBlock(pblocktemplate), true);
        num_txs_for_next_block = 0;
        nNextHeight = chainActive.Height() + 1;
    }
    setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight - 1));
}

// disconnect i blocks from tip
void ClaimTrieChainFixture::DecrementBlocks(int num_blocks)
{
    clear(); // clears the internal cache
    CValidationState state;
    {
        LOCK(cs_main);
        CBlockIndex* pblockindex = chainActive[chainActive.Height() - num_blocks + 1];
        BOOST_CHECK_EQUAL(InvalidateBlock(state, Params(), pblockindex), true);
    }
    BOOST_CHECK_EQUAL(state.IsValid(), true);
    BOOST_CHECK_EQUAL(ActivateBestChain(state, Params()), true);
    mempool.clear();
    num_txs_for_next_block = 0;
    nNextHeight = chainActive.Height() + 1;
    setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight - 1));
}

// decrement back to last mark
void ClaimTrieChainFixture::DecrementBlocks()
{
    int mark = marks.back();
    marks.pop_back();
    DecrementBlocks(chainActive.Height() - mark);
}

template <typename K>
bool ClaimTrieChainFixture::keyTypeEmpty(uint8_t keyType)
{
    boost::scoped_ptr<CDBIterator> pcursor(base->db->NewIterator());
    pcursor->SeekToFirst();

    while (pcursor->Valid()) {
        std::pair<uint8_t, K> key;
        if (pcursor->GetKey(key))
            if (key.first == keyType)
                return false;
        pcursor->Next();
    }
    return true;
}

bool ClaimTrieChainFixture::queueEmpty()
{
    for (const auto& claimQueue: claimQueueCache)
        if (!claimQueue.second.empty())
            return false;
    return keyTypeEmpty<int>(CLAIM_QUEUE_ROW);
}

bool ClaimTrieChainFixture::expirationQueueEmpty()
{
    for (const auto& expirationQueue: expirationQueueCache)
        if (!expirationQueue.second.empty())
            return false;
    return keyTypeEmpty<int>(CLAIM_EXP_QUEUE_ROW);
}

bool ClaimTrieChainFixture::supportEmpty()
{
    for (const auto& entry: supportCache)
        if (!entry.second.empty())
            return false;
    return supportCache.empty() && keyTypeEmpty<std::string>(SUPPORT);
}

bool ClaimTrieChainFixture::supportQueueEmpty()
{
    for (const auto& support: supportQueueCache)
        if (!support.second.empty())
            return false;
    return keyTypeEmpty<int>(SUPPORT_QUEUE_ROW);
}

int ClaimTrieChainFixture::proportionalDelayFactor()
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
boost::test_tools::predicate_result ClaimTrieChainFixture::is_claim_in_queue(const std::string& name, const CTransaction &tx)
{
    COutPoint outPoint(tx.GetHash(), 0);
    int validAtHeight;
    if (haveClaimInQueue(name, outPoint, validAtHeight))
        return true;
    return negativeResult("Is not a claim in queue");
}

// check if tx is best claim based on outpoint
boost::test_tools::predicate_result ClaimTrieChainFixture::is_best_claim(const std::string& name, const CTransaction &tx)
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

std::size_t ClaimTrieChainFixture::getTotalNamesInTrie() const
{
    return base->getTotalNamesInTrie();
}
