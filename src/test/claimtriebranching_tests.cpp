// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "consensus/validation.h"
#include "consensus/merkle.h"
#include "primitives/transaction.h"
#include "miner.h"
#include "txmempool.h"
#include "claimtrie.h"
#include "nameclaim.h"
#include "coins.h"
#include "streams.h"
#include "chainparams.h"
#include "policy/policy.h"
#include <boost/test/unit_test.hpp>
#include <iostream>
#include "test/test_bitcoin.h"

using namespace std;


BOOST_FIXTURE_TEST_SUITE(claimtriebranching_tests, RegTestingSetup)

//is a claim in queue
boost::test_tools::predicate_result
is_claim_in_queue(std::string name, const CTransaction &tx)
{
    CClaimValue val;
    COutPoint outPoint(tx.GetHash(), 0);
    int validAtHeight;
    bool have_claim = pclaimTrie->haveClaimInQueue(name,outPoint,validAtHeight);
    if (have_claim){
        return true;
    }
    else{
        boost::test_tools::predicate_result res(false);
        res.message()<<"Is not a claim in queue.";
        return res;
    }
}

// check if tx is best claim based on outpoint
boost::test_tools::predicate_result
is_best_claim(std::string name, const CTransaction &tx)
{
    CClaimValue val;
    COutPoint outPoint(tx.GetHash(), 0);
    bool have_claim = pclaimTrie->haveClaim(name,outPoint);
    bool have_info = pclaimTrie->getInfoForName(name, val);
    if (have_claim && have_info && val.outPoint == outPoint){
        return true;
    }
    else{
        boost::test_tools::predicate_result res(false);
        res.message()<<"Is not best claim";
        return res;
    }
}
// check effective quantity of best claim
boost::test_tools::predicate_result
best_claim_effective_amount_equals(std::string name, CAmount amount)
{
    CClaimValue val;
    bool have_info = pclaimTrie->getInfoForName(name, val);
    if (!have_info)
    {
        boost::test_tools::predicate_result res(false);
        res.message()<<"No claim found";
        return res;
    }
    else
    {
        CAmount effective_amount = pclaimTrie->getEffectiveAmountForClaim(name, val.claimId);
        if (effective_amount != amount)
        {
            boost::test_tools::predicate_result res(false);
            res.message()<<amount<<" != "<<effective_amount;
            return res;
        }
        else
        {
            return true;
        }
    }

}

CMutableTransaction BuildTransaction(const CMutableTransaction& prev, uint32_t prevout=0, unsigned int numOutputs=1)
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(numOutputs);
    tx.vin[0].prevout.hash = prev.GetHash();
    tx.vin[0].prevout.n = prevout;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    CAmount valuePerOutput = prev.vout[prevout].nValue;
    unsigned int numOutputsCopy = numOutputs;
    while ((numOutputsCopy = numOutputsCopy >> 1) > 0)
    {
        valuePerOutput = valuePerOutput >> 1;
    }
    for (unsigned int i = 0; i < numOutputs; ++i)
    {
        tx.vout[i].scriptPubKey = CScript();
        tx.vout[i].nValue = valuePerOutput;
    }

    return tx;
}
bool CreateBlock(CBlockTemplate* pblocktemplate)
{
    static int unique_block_counter = 0;
    CBlock* pblock = &pblocktemplate->block;
    pblock->nVersion = 1;
    pblock->nTime = chainActive.Tip()->GetBlockTime()+Params().GetConsensus().nPowTargetSpacing;
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = CScript() << CScriptNum(unique_block_counter++) << CScriptNum(chainActive.Height());
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
    pblock->vtx[0] = CTransaction(txCoinbase);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    for (int i = 0; ; ++i)
    {
        pblock->nNonce = i;
        if (CheckProofOfWork(pblock->GetPoWHash(), pblock->nBits, Params().GetConsensus()))
        {
            break;
        }
    }
    CValidationState state;
    bool success = (ProcessNewBlock(state, Params(), NULL, pblock, true, NULL) && state.IsValid() && pblock->GetHash() == chainActive.Tip()->GetBlockHash());
    pblock->hashPrevBlock = pblock->GetHash();
    return success;

}

bool CreateCoinbases(unsigned int num_coinbases, std::vector<CTransaction>& coinbases)
{
    CBlockTemplate *pblocktemplate;
    coinbases.clear();
    BOOST_CHECK(pblocktemplate = CreateNewBlock(Params(), CScript()<<OP_TRUE ));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 1);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 100 + num_coinbases; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
        if (coinbases.size() < num_coinbases)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }
    delete pblocktemplate;
    return true;
}


// Test Fixtures
struct ClaimTrieChainFixture{
    std::vector<CTransaction> coinbase_txs;
    int coinbase_txs_used;
    unsigned int num_txs;
    unsigned int num_txs_for_next_block;

    // these will take on regtest parameters
    const int expirationForkHeight;
    const int originalExpiration;
    const int extendedExpiration;


    ClaimTrieChainFixture():
        expirationForkHeight(Params(CBaseChainParams::REGTEST).GetConsensus().nExtendedClaimExpirationForkHeight),
        originalExpiration(Params(CBaseChainParams::REGTEST).GetConsensus().nOriginalClaimExpirationTime),
        extendedExpiration(Params(CBaseChainParams::REGTEST).GetConsensus().nExtendedClaimExpirationTime)
    {
        fRequireStandard = false;
        BOOST_CHECK(pclaimTrie->nNextHeight == chainActive.Height() + 1);
        LOCK(cs_main);
        num_txs_for_next_block = 0;
        num_txs = 0;
        coinbase_txs_used = 0;
        // generate coinbases to spend
        CreateCoinbases(20,coinbase_txs);
    }

    ~ClaimTrieChainFixture()
    {
        DecrementBlocks(chainActive.Height());
    }


    void CommitTx(CMutableTransaction &tx){
        num_txs_for_next_block++;
        num_txs++;
        if(num_txs > coinbase_txs.size())
        {
            //ran out of coinbases to spend
            assert(0);
        }

        CValidationState state;
        bool *fMissingInputs;
        CFeeRate txFeeRate = CFeeRate(0);
        BOOST_CHECK(AcceptToMemoryPool(mempool, state, tx, false, fMissingInputs, &txFeeRate));

    }

    //spend a bid into some non claimtrie related unspent
    CMutableTransaction Spend(const CTransaction &prev){

        uint32_t prevout = 0;
        CMutableTransaction tx = BuildTransaction(prev,prevout);
        tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
        tx.vout[0].nValue = 1;

        CommitTx(tx);
        return tx;
    }


    //make claim at the current block
    CMutableTransaction MakeClaim(const CTransaction &prev, std::string name, std::string value,
                                  CAmount quantity)
    {
        uint32_t prevout = 0;

        CMutableTransaction tx = BuildTransaction(prev,prevout);
        tx.vout[0].scriptPubKey = ClaimNameScript(name,value);
        tx.vout[0].nValue = quantity;

        CommitTx(tx);
        return tx;
    }

    //make support at the current block
    CMutableTransaction MakeSupport(const CTransaction &prev, const CTransaction &claimtx, std::string name, CAmount quantity)
    {
        uint160 claimId = ClaimIdHash(claimtx.GetHash(), 0);
        uint32_t prevout = 0;

        CMutableTransaction tx = BuildTransaction(prev,prevout);
        tx.vout[0].scriptPubKey = SupportClaimScript(name,claimId);
        tx.vout[0].nValue = quantity;

        CommitTx(tx);
        return tx;
    }

    //make update at the current block
    CMutableTransaction MakeUpdate(const CTransaction &prev, std::string name, std::string value,
                               uint160 claimId, CAmount quantity)
    {
        uint32_t prevout = 0;

        CMutableTransaction tx = BuildTransaction(prev,prevout);
        tx.vout[0].scriptPubKey = UpdateClaimScript(name,claimId,value);
        tx.vout[0].nValue = quantity;

        CommitTx(tx);
        return tx;
    }

    CMutableTransaction GetCoinbase()
    {
        CMutableTransaction tx = coinbase_txs[coinbase_txs_used];
        coinbase_txs_used++;
        return tx;
    }

    //create i blocks
    void IncrementBlocks(int num_blocks)
    {
        for (int i = 0; i < num_blocks; ++i)
        {
            CBlockTemplate *pblocktemplate;
            CScript coinbase_scriptpubkey;
            coinbase_scriptpubkey << CScriptNum(chainActive.Height());
            BOOST_CHECK(pblocktemplate = CreateNewBlock(Params(), coinbase_scriptpubkey));
            BOOST_CHECK(pblocktemplate->block.vtx.size() == num_txs_for_next_block+1);
            pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
            BOOST_CHECK(CreateBlock(pblocktemplate));
            delete pblocktemplate;

            num_txs_for_next_block = 0 ;
        }

    }

    //disconnect i blocks from tip
    void DecrementBlocks(int num_blocks)
    {
        for(int i = 0; i< num_blocks; i++){
            CValidationState state;
            CBlockIndex* pblockindex = chainActive.Tip();
            InvalidateBlock(state, Params().GetConsensus(), pblockindex);
            if (state.IsValid())
            {
                ActivateBestChain(state, Params());
            }
            else
            {
                BOOST_FAIL("removing block failed");
            }

        }
        mempool.clear();
        num_txs_for_next_block = 0;
    }


    void WriteClearReadClaimTrie()
    {
        // this will simulate restart of lbrycrdd by writing the claimtrie to disk,
        // clearing the-in memory claimtrie, and then reading the saved claimtrie
        // from disk
        pclaimTrie->WriteToDisk();
        pclaimTrie->clear();
        BOOST_CHECK(pclaimTrie->ReadFromDisk(true));
    }

};


/*
    claims
        no competing bids
        there is a competing bid inserted same height
            check the greater one wins
                - quantity is same, check outpoint greater wins
        there is an existing competing bid
            check that rules for delays are observed
            check that a greater amount wins
            check that a smaller amount does not win

*/
BOOST_AUTO_TEST_CASE(claimtriebranching_claim)
{
    // no competing bids
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(!is_best_claim("test",tx1));


    // there is a competing bid inserted same height
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx3));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(!is_best_claim("test",tx2));
    BOOST_CHECK(!is_best_claim("test",tx3));

    // make two claims , one older
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx4));
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(),"test","two",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_claim_in_queue("test",tx5));
    BOOST_CHECK(is_best_claim("test",tx4));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx4));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx4));
    BOOST_CHECK(is_claim_in_queue("test",tx5));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx4));
    fixture.DecrementBlocks(1);

    // check claim takeover
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx6));
    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(),"test","two",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_claim_in_queue("test",tx7));
    BOOST_CHECK(is_best_claim("test",tx6));
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx7));

    fixture.DecrementBlocks(10);
    BOOST_CHECK(is_claim_in_queue("test",tx7));
    BOOST_CHECK(is_best_claim("test",tx6));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx6));
    fixture.DecrementBlocks(10);

}

/*
    spent claims
        spending winning claim will make losing active claim winner
        spending winning claim will make inactive claim winner
        spending winning claim will empty out claim trie
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_spend_claim)
{
    ClaimTrieChainFixture fixture;

    // spending winning claim will make losing active claim winner
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));
    fixture.Spend(tx1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));
    fixture.DecrementBlocks(1);


    // spending winning claim will make inactive claim winner
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx3));
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx3));
    fixture.Spend(tx3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx4));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx3));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx3));
    fixture.DecrementBlocks(10);


    //spending winning claim will empty out claim trie
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx5));
    fixture.Spend(tx5);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!is_best_claim("test",tx5));
    BOOST_CHECK(pclaimTrie->empty());

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx5));
    fixture.DecrementBlocks(1);
}


/*
    supports
        check support with wrong name does not work
        check claim with more support wins
        check support delay
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_support)
{
    ClaimTrieChainFixture fixture;
    // check claim with more support wins
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(),tx2,"test",10);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx2));
    BOOST_CHECK(best_claim_effective_amount_equals("test",11));
    fixture.DecrementBlocks(1);

    // check support delay
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(),"test","two",2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx4));
    BOOST_CHECK(best_claim_effective_amount_equals("test",2));
    CMutableTransaction s4 = fixture.MakeSupport(fixture.GetCoinbase(),tx3,"test",10); //10 delay
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx4));
    BOOST_CHECK(best_claim_effective_amount_equals("test",2));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx3));
    BOOST_CHECK(best_claim_effective_amount_equals("test",11));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx4));
    BOOST_CHECK(best_claim_effective_amount_equals("test",2));
    fixture.DecrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx4));
    BOOST_CHECK(best_claim_effective_amount_equals("test",2));
    fixture.DecrementBlocks(10);
}

/*
    support on abandon
        supporting a claim the same block it gets abandoned,
        or abandoing a support the same block claim gets abandoned,
        when there were no other claims would crash lbrycrd,
        make sure this doesn't happen in below three tests
        (https://github.com/lbryio/lbrycrd/issues/77)
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_support_on_abandon)
{
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);

    //supporting and abandoing on the same block will cause it to crash
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    fixture.Spend(tx1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!is_best_claim("test",tx1));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));

}

BOOST_AUTO_TEST_CASE(claimtriebranching_support_on_abandon_2)
{
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    fixture.IncrementBlocks(1);

    //abandoning a support and abandoing claim on the same block will cause it to crash
    fixture.Spend(tx1);
    fixture.Spend(s1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!is_best_claim("test",tx1));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));
}

BOOST_AUTO_TEST_CASE(claimtriebranching_update_on_support)
{
    // make sure that support on abandon bug does not affect
    // updates happening on the same block as a support
    // (it should not)
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",3);
    fixture.IncrementBlocks(1);

    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    CMutableTransaction u1 = fixture.MakeUpdate(tx1,"test","two",ClaimIdHash(tx1.GetHash(),0),1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(is_best_claim("test",u1));
    BOOST_CHECK(best_claim_effective_amount_equals("test",2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));
}



/*
    support spend
        spending suport on winning claim will cause it to lose

        spending a support on txin[i] where i is not 0
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_support_spend)
{

    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));
    CMutableTransaction sp1 = fixture.Spend(s1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));
    fixture.DecrementBlocks(1);

    // spend a support on txin[i] where i is not 0

    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"x","one",3);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(),"test","two",2);
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(),"test","three",1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(),tx5,"test",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx5));


    // build the spend where s2 is sppent on txin[1] and tx3 is  spent on txin[0]
    uint32_t prevout = 0;
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(2);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = tx3.GetHash();
    tx.vin[0].prevout.n = prevout;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vin[1].prevout.hash = s2.GetHash();
    tx.vin[1].prevout.n = prevout;
    tx.vin[1].scriptSig = CScript();
    tx.vin[1].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    tx.vout[0].nValue = 1;

    fixture.CommitTx(tx);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(is_best_claim("test",tx4));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx5));

}
/*
    update
        update preserves claim id
        update preserves supports
        winning update on winning claim happens without delay
        losing update on winning claim happens without delay
        update on losing claim happens with delay , and wins


*/
BOOST_AUTO_TEST_CASE(claimtriebranching_update)
{
    //update preserves claim id
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction u1 = fixture.MakeUpdate(tx1,"test","one",ClaimIdHash(tx1.GetHash(),0),2);
    fixture.IncrementBlocks(1);
    CClaimValue val;
    pclaimTrie->getInfoForName("test",val);
    BOOST_CHECK(val.claimId == ClaimIdHash(tx1.GetHash(),0));
    BOOST_CHECK(is_best_claim("test",u1));
    fixture.DecrementBlocks(1);

    // update preserves supports
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx2,"test",1);
    CMutableTransaction u2 = fixture.MakeUpdate(tx2,"test","one",ClaimIdHash(tx2.GetHash(),0),1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(best_claim_effective_amount_equals("test",2));
    fixture.DecrementBlocks(1);

    // winning update on winning claim happens without delay
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(10);
    CMutableTransaction u3 = fixture.MakeUpdate(tx3,"test","one",ClaimIdHash(tx3.GetHash(),0),2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",u3));
    fixture.DecrementBlocks(11);

    // losing update on winning claim happens without delay
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",3);
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx5));
    CMutableTransaction u4 = fixture.MakeUpdate(tx5,"test","one",ClaimIdHash(tx5.GetHash(),0),1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx6));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx5));
    fixture.DecrementBlocks(10);

    // update on losing claim happens with delay , and wins
    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",3);
    CMutableTransaction tx8 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx7));

    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(2);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = tx8.GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vin[1].prevout.hash = fixture.GetCoinbase().GetHash();
    tx.vin[1].prevout.n = 0;
    tx.vin[1].scriptSig = CScript();
    tx.vin[1].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = UpdateClaimScript("test",ClaimIdHash(tx8.GetHash(),0),"one");
    tx.vout[0].nValue = 4;
    fixture.CommitTx(tx);

    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx7));
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx));

    fixture.DecrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx7));
    fixture.DecrementBlocks(11);

}

/*
    expiration
        check claims expire and loses claim
        check claims expire and is not updateable (may be changed in future soft fork)
        check supports expire and can cause supported bid to lose claim
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_expire)
{
    ClaimTrieChainFixture fixture;
    pclaimTrie->setExpirationTime(5);

    // check claims expire and loses claim
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(pclaimTrie->nExpirationTime);
    BOOST_CHECK(is_best_claim("test",tx1));
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx1));
    fixture.DecrementBlocks(pclaimTrie->nExpirationTime);

    // check claims expire and is not updateable (may be changed in future soft fork)
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx3));
    fixture.IncrementBlocks(pclaimTrie->nExpirationTime);
    CMutableTransaction u1 = fixture.MakeUpdate(tx3,"test","two",ClaimIdHash(tx3.GetHash(),0) ,2);
    BOOST_CHECK(!is_best_claim("test",u1));

    fixture.DecrementBlocks(pclaimTrie->nExpirationTime);
    BOOST_CHECK(is_best_claim("test",tx3));
    fixture.DecrementBlocks(1);

    // check supports expire and can cause supported bid to lose claim
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx4,"test",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx4));
    CMutableTransaction u2 = fixture.MakeUpdate(tx4,"test","two",ClaimIdHash(tx4.GetHash(),0) ,1);
    CMutableTransaction u3 = fixture.MakeUpdate(tx5,"test","two",ClaimIdHash(tx5.GetHash(),0) ,2);
    fixture.IncrementBlocks(pclaimTrie->nExpirationTime);
    BOOST_CHECK(is_best_claim("test",u3));
    fixture.DecrementBlocks(pclaimTrie->nExpirationTime);
    BOOST_CHECK(is_best_claim("test",tx4));
    fixture.DecrementBlocks(1);

    // check updated claims will extend expiration
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",tx6));
    CMutableTransaction u4 = fixture.MakeUpdate(tx6,"test","two",ClaimIdHash(tx6.GetHash(),0) ,2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",u4));
    fixture.IncrementBlocks(pclaimTrie->nExpirationTime-1);
    BOOST_CHECK(is_best_claim("test",u4));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!is_best_claim("test",u4));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",u4));
    fixture.DecrementBlocks(pclaimTrie->nExpirationTime);
    BOOST_CHECK(is_best_claim("test",tx6));


}

/*
 * tests for CClaimTrie::getEffectiveAmountForClaim
 */
BOOST_AUTO_TEST_CASE(claimtriebranching_get_effective_amount_for_claim)
{
    // simplest scenario. One claim, no supports
    ClaimTrieChainFixture fixture;
    CMutableTransaction claimtx = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    uint160 claimId = ClaimIdHash(claimtx.GetHash(), 0);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("test", claimId) == 2);
    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("inexistent", claimId) == 0); //not found returns 0

    // one claim, one support
    fixture.MakeSupport(fixture.GetCoinbase(), claimtx, "test", 40);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("test", claimId) == 42);

    // Two claims, first one with supports
    CMutableTransaction claimtx2 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "two", 1);
    uint160 claimId2 = ClaimIdHash(claimtx2.GetHash(), 0);
    fixture.IncrementBlocks(10);

    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("test", claimId) == 42);
    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("test", claimId2) == 1);
    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("inexistent", claimId) == 0);
    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("inexistent", claimId2) == 0);

    // Two claims, both with supports, second claim effective amount being less than first claim
    fixture.MakeSupport(fixture.GetCoinbase(), claimtx2, "test", 6);
    fixture.IncrementBlocks(13); //delay

    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("test", claimId) == 42);
    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("test", claimId2) == 7);

    // Two claims, both with supports, second one taking over
    fixture.MakeSupport(fixture.GetCoinbase(), claimtx2, "test", 1330);
    fixture.IncrementBlocks(26); //delay

    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("test", claimId) == 42);
    BOOST_CHECK(pclaimTrie->getEffectiveAmountForClaim("test", claimId2) == 1337);
}

/*
 * tests for CClaimTrie::getClaimById basic consistency checks
 */
BOOST_AUTO_TEST_CASE(claimtriebranching_get_claim_by_id)
{
    ClaimTrieChainFixture fixture;
    const std::string name = "test";
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    fixture.IncrementBlocks(1);

    CClaimValue claimValue;
    std::string claimName;
    pclaimTrie->getClaimById(claimId, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId);

    fixture.MakeSupport(fixture.GetCoinbase(), tx1, name, 4);
    fixture.IncrementBlocks(1);

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), name, "two", 1);
    uint160 claimId2 = ClaimIdHash(tx2.GetHash(), 0);
    fixture.IncrementBlocks(1);

    pclaimTrie->getClaimById(claimId2, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId2);

    fixture.DecrementBlocks(1);

    CClaimValue claimValue2;
    claimName = "";
    pclaimTrie->getClaimById(claimId2, claimName, claimValue2);
    BOOST_CHECK(claimName != name);
    BOOST_CHECK(claimValue2.claimId != claimId2);

    pclaimTrie->getClaimById(claimId, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId);

    fixture.DecrementBlocks(2);

    claimName = "";
    pclaimTrie->getClaimById(claimId, claimName, claimValue2);
    BOOST_CHECK(claimName != name);
    BOOST_CHECK(claimValue2.claimId != claimId);
}

/*
    claim expiration for hard fork
        check claims do not expire post ExpirationForkHeight
        check supports work post ExpirationForkHeight
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_hardfork_claim)
{
    ClaimTrieChainFixture fixture;

    BOOST_CHECK_EQUAL(pclaimTrie->nExpirationTime, fixture.originalExpiration);
    // First create a claim and make sure it expires pre-fork
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",3);
    fixture.IncrementBlocks(fixture.originalExpiration+1);
    BOOST_CHECK(!is_best_claim("test",tx1));
    fixture.DecrementBlocks(fixture.originalExpiration);
    BOOST_CHECK(is_best_claim("test",tx1));
    fixture.IncrementBlocks(fixture.originalExpiration);
    BOOST_CHECK(!is_best_claim("test",tx1));

    // Create a claim 1 block before the fork height that will expire after the fork height
    fixture.IncrementBlocks(fixture.expirationForkHeight - chainActive.Height() -2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test2","one",3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(chainActive.Height(), fixture.expirationForkHeight -1);

    // Disable future expirations and fast-forward past the fork height
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(chainActive.Height(), fixture.expirationForkHeight);
    BOOST_CHECK_EQUAL(pclaimTrie->nExpirationTime, fixture.extendedExpiration);
    // make sure decrementing to before the fork height will apppropriately set back the
    // expiration time to the original expiraiton time
    fixture.DecrementBlocks(1);
    BOOST_CHECK_EQUAL(pclaimTrie->nExpirationTime, fixture.originalExpiration);
    fixture.IncrementBlocks(1);

    // make sure that claim created 1 block before the fork expires as expected
    // at the extended expiration times
    BOOST_CHECK(is_best_claim("test2", tx2));
    fixture.IncrementBlocks(fixture.extendedExpiration-1);
    BOOST_CHECK(!is_best_claim("test2", tx2));
    fixture.DecrementBlocks(fixture.extendedExpiration-1);

    // This first claim is still expired since it's pre-fork, even
    // after fork activation
    BOOST_CHECK(!is_best_claim("test",tx1));

    // This new claim created at the fork height cannot expire at original expiration
    BOOST_CHECK_EQUAL(chainActive.Height(), fixture.expirationForkHeight);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(1);
    fixture.IncrementBlocks(fixture.originalExpiration);
    BOOST_CHECK(is_best_claim("test",tx3));
    BOOST_CHECK(!is_best_claim("test",tx1));
    fixture.DecrementBlocks(fixture.originalExpiration);

    // but it expires at the extended expiration, and not a single block below
    fixture.IncrementBlocks(fixture.extendedExpiration);
    BOOST_CHECK(!is_best_claim("test",tx3));
    fixture.DecrementBlocks(fixture.extendedExpiration);
    fixture.IncrementBlocks(fixture.extendedExpiration-1);
    BOOST_CHECK(is_best_claim("test",tx3));
    fixture.DecrementBlocks(fixture.extendedExpiration-1);


    // Ensure that we cannot update the original pre-fork expired claim
    CMutableTransaction u1 = fixture.MakeUpdate(tx1,"test","two",ClaimIdHash(tx1.GetHash(),0), 3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!is_best_claim("test",u1));

    // Ensure that supports for the expired claim don't support it
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),u1,"test",10);
    BOOST_CHECK(!is_best_claim("test",u1));

    // Ensure that we can update the new post-fork claim
    CMutableTransaction u2 = fixture.MakeUpdate(tx3,"test","two",ClaimIdHash(tx3.GetHash(),0), 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_best_claim("test",u2));

    // Ensure that supports for the new post-fork claim
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(),u2,"test",3);
    BOOST_CHECK(is_best_claim("test",u2));
}

/*
    support expiration for hard fork
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_hardfork_support)
{
    ClaimTrieChainFixture fixture;

    int blocks_before_fork = 10;
    fixture.IncrementBlocks(fixture.expirationForkHeight - chainActive.Height() - blocks_before_fork-1);
    // Create claim and support it before the fork height
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",2);
    // this claim will win without the support
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);
    fixture.IncrementBlocks(blocks_before_fork);

    // check that the claim expires as expected at the extended time, as does the support
    fixture.IncrementBlocks(fixture.originalExpiration - blocks_before_fork);
    BOOST_CHECK(is_best_claim("test",tx1));
    BOOST_CHECK(best_claim_effective_amount_equals("test",3));
    fixture.DecrementBlocks(fixture.originalExpiration - blocks_before_fork);
    fixture.IncrementBlocks(fixture.extendedExpiration - blocks_before_fork);
    BOOST_CHECK(!is_best_claim("test",tx1));
    fixture.DecrementBlocks(fixture.extendedExpiration - blocks_before_fork);
    fixture.IncrementBlocks(fixture.extendedExpiration - blocks_before_fork - 1);
    BOOST_CHECK(is_best_claim("test",tx1));
    BOOST_CHECK(best_claim_effective_amount_equals("test",3));
    fixture.DecrementBlocks(fixture.extendedExpiration - blocks_before_fork - 1);

    // update the claims at fork
    fixture.DecrementBlocks(1);
    CMutableTransaction u1 = fixture.MakeUpdate(tx1,"test","two",ClaimIdHash(tx1.GetHash(),0),1);
    CMutableTransaction u2 = fixture.MakeUpdate(tx2,"test","two",ClaimIdHash(tx2.GetHash(),0),2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(fixture.expirationForkHeight, chainActive.Height());

    BOOST_CHECK(is_best_claim("test", u1));
    BOOST_CHECK(best_claim_effective_amount_equals("test",3));
    BOOST_CHECK(!is_claim_in_queue("test",tx1));
    BOOST_CHECK(!is_claim_in_queue("test",tx2));

    // check that the support expires as expected
    fixture.IncrementBlocks(fixture.extendedExpiration - blocks_before_fork);
    BOOST_CHECK(is_best_claim("test",u2));
    fixture.DecrementBlocks(fixture.extendedExpiration - blocks_before_fork);
    fixture.IncrementBlocks(fixture.extendedExpiration - blocks_before_fork - 1);
    BOOST_CHECK(is_best_claim("test",u1));
    BOOST_CHECK(best_claim_effective_amount_equals("test",3));

}

/*
    claim/support expiration for hard fork, but with checks for disk procedures
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_hardfork_disktest)
{
    ClaimTrieChainFixture fixture;

    // Check that incrementing to fork height, reseting to disk will get proper expiration time
    BOOST_CHECK_EQUAL(pclaimTrie->nExpirationTime, fixture.originalExpiration);
    fixture.IncrementBlocks(fixture.expirationForkHeight - chainActive.Height());
    BOOST_CHECK_EQUAL(chainActive.Height(), fixture.expirationForkHeight);
    BOOST_CHECK_EQUAL(pclaimTrie->nExpirationTime, fixture.extendedExpiration);
    fixture.WriteClearReadClaimTrie();
    BOOST_CHECK_EQUAL(pclaimTrie->nExpirationTime, fixture.extendedExpiration);

    // Create a claim and support 1 block before the fork height that will expire after the fork height.
    // Reset to disk, increment past the fork height and make sure we get
    // proper behavior
    fixture.DecrementBlocks(2);
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(chainActive.Height(), fixture.expirationForkHeight -1);

    fixture.WriteClearReadClaimTrie();
    BOOST_CHECK_EQUAL(chainActive.Height(), fixture.expirationForkHeight-1);
    BOOST_CHECK_EQUAL(pclaimTrie->nExpirationTime, fixture.originalExpiration);
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(pclaimTrie->nExpirationTime, fixture.extendedExpiration);
    BOOST_CHECK(is_best_claim("test", tx1));
    BOOST_CHECK(best_claim_effective_amount_equals("test",2));
    fixture.IncrementBlocks(fixture.originalExpiration-1);
    BOOST_CHECK(is_best_claim("test", tx1));
    BOOST_CHECK(best_claim_effective_amount_equals("test",2));
    fixture.DecrementBlocks(fixture.originalExpiration-1);
    fixture.IncrementBlocks(fixture.extendedExpiration-1);
    BOOST_CHECK(!is_best_claim("test", tx1));

    // Create a claim and support before the fork height, reset to disk, update the claim
    // increment past the fork height and make sure we get proper behavior
    int  height_of_update_before_expiration = 50;
    fixture.DecrementBlocks(chainActive.Height() - fixture.expirationForkHeight + height_of_update_before_expiration+2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test2","one",1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(),tx2,"test2",1);
    fixture.IncrementBlocks(1);
    fixture.WriteClearReadClaimTrie();
    CMutableTransaction u2 = fixture.MakeUpdate(tx2,"test2","two",ClaimIdHash(tx2.GetHash(),0),1);
    // increment to fork
    fixture.IncrementBlocks(fixture.expirationForkHeight - chainActive.Height());
    BOOST_CHECK(is_best_claim("test2", u2));
    BOOST_CHECK(best_claim_effective_amount_equals("test2",2));
    // increment to original expiration, should not be expired
    fixture.IncrementBlocks(fixture.originalExpiration - height_of_update_before_expiration);
    BOOST_CHECK(is_best_claim("test2", u2));
    BOOST_CHECK(best_claim_effective_amount_equals("test2",2));
    fixture.DecrementBlocks(fixture.originalExpiration - height_of_update_before_expiration);
    // increment to extended expiration, should be expired and not one block before
    fixture.IncrementBlocks(fixture.extendedExpiration - height_of_update_before_expiration);
    BOOST_CHECK(!is_best_claim("test2", u2));
    fixture.DecrementBlocks(fixture.extendedExpiration - height_of_update_before_expiration);
    fixture.IncrementBlocks(fixture.extendedExpiration - height_of_update_before_expiration-1);
    BOOST_CHECK(is_best_claim("test2", u2));
    BOOST_CHECK(best_claim_effective_amount_equals("test2",1)); // the support expires one block before
}

BOOST_AUTO_TEST_SUITE_END()
