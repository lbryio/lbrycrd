// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "chainparams.h"
#include "claimtrie.h"
#include "coins.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "main.h"
#include "miner.h"
#include "nameclaim.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "streams.h"
#include "test/test_bitcoin.h"
#include "txmempool.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using namespace std;


BOOST_FIXTURE_TEST_SUITE(claimtriebranching_tests, RegTestingSetup)

//is a claim in queue 
boost::test_tools::predicate_result
is_claim_in_queue(std::string name, const CTransaction &tx)
{
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
    for (uint32_t i = 0;; ++i) {
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
    std::vector<int> marks;
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
        ENTER_CRITICAL_SECTION(cs_main);
        BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);
        num_txs_for_next_block = 0;
        num_txs = 0; 
        coinbase_txs_used = 0;
        // generate coinbases to spend
        CreateCoinbases(40, coinbase_txs);
    }

    ~ClaimTrieChainFixture()
    {
        DecrementBlocks(chainActive.Height());
        LEAVE_CRITICAL_SECTION(cs_main);
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
        bool fMissingInputs;
        CFeeRate txFeeRate = CFeeRate(0);
        BOOST_CHECK(AcceptToMemoryPool(mempool, state, tx, false, &fMissingInputs, &txFeeRate));
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

    CMutableTransaction MakeClaim(const CTransaction& prev, std::string name, std::string value)
    {
        return MakeClaim(prev, name, value, prev.vout[0].nValue);
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
    void IncrementBlocks(int num_blocks, bool mark = false)
    {
        if (mark)
            marks.push_back(chainActive.Height());

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
        for (int i = 0; i < num_blocks; i++) {
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

    // decrement back to last mark
    void DecrementBlocks()
    {
        int mark = marks.back();
        marks.pop_back();
        DecrementBlocks(chainActive.Height() - mark);
    }

    void WriteClearReadClaimTrie()
    {
        // this will simulate restart of lbrycrdd by writing the claimtrie to disk,
        // clearing the-in memory claimtrie, and then reading the saved claimtrie
        // from disk
        pclaimTrie->WriteToDisk();
        pclaimTrie->clear();
        pclaimTrie->ReadFromDisk(true);
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
BOOST_AUTO_TEST_CASE(claim_test)
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
    BOOST_CHECK_EQUAL(2U, pclaimTrie->getClaimsForName("test").claims.size());

    fixture.DecrementBlocks(1); 
    BOOST_CHECK(!is_best_claim("test",tx2));
    BOOST_CHECK(!is_best_claim("test",tx3));
    BOOST_CHECK_EQUAL(0U, pclaimTrie->getClaimsForName("test").claims.size());

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
    BOOST_CHECK_EQUAL(2U, pclaimTrie->getClaimsForName("test").claims.size());

    fixture.DecrementBlocks(1); 
    BOOST_CHECK(is_best_claim("test",tx4)); 
    BOOST_CHECK(is_claim_in_queue("test",tx5));
    fixture.DecrementBlocks(1); 
    BOOST_CHECK(is_best_claim("test",tx4)); 
    fixture.DecrementBlocks(1);

    // check claim takeover, note that CClaimTrie.nProportionalDelayFactor is set to 1
    // instead of 32 in test_bitcoin.cpp
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(10); 
    BOOST_CHECK(is_best_claim("test",tx6));
    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(),"test","two",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(is_claim_in_queue("test",tx7));
    BOOST_CHECK(is_best_claim("test", tx6));
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test",tx7));
    BOOST_CHECK_EQUAL(2U, pclaimTrie->getClaimsForName("test").claims.size());

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
BOOST_AUTO_TEST_CASE(spend_claim_test)
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
BOOST_AUTO_TEST_CASE(support_test)
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
BOOST_AUTO_TEST_CASE(support_on_abandon_test)
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

BOOST_AUTO_TEST_CASE(support_on_abandon_2_test)
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

BOOST_AUTO_TEST_CASE(update_on_support_test)
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
    BOOST_CHECK(is_best_claim("test", tx1));
}

/*
    support spend
        spending suport on winning claim will cause it to lose

        spending a support on txin[i] where i is not 0 
*/
BOOST_AUTO_TEST_CASE(support_spend_test)
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
    BOOST_CHECK_EQUAL(2U, pclaimTrie->getClaimsForName("test").claims.size());

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
BOOST_AUTO_TEST_CASE(claimtrie_update_test)
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
    BOOST_CHECK_EQUAL(2U, pclaimTrie->getClaimsForName("test").claims.size());
    fixture.DecrementBlocks(11);

    // losing update on winning claim happens without delay
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",3);
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(is_best_claim("test", tx5));
    BOOST_CHECK_EQUAL(2U, pclaimTrie->getClaimsForName("test").claims.size());
    CMutableTransaction u4 = fixture.MakeUpdate(tx5, "test", "one", ClaimIdHash(tx5.GetHash(), 0), 1);
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
BOOST_AUTO_TEST_CASE(claimtrie_expire_test)
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
BOOST_AUTO_TEST_CASE(get_claim_by_id_test)
{
    ClaimTrieChainFixture fixture;
    const std::string name = "test";
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 2);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    fixture.IncrementBlocks(1);

    CClaimValue claimValue;
    std::string claimName;
    pclaimTrie->getClaimById(claimId, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId);

    fixture.MakeSupport(fixture.GetCoinbase(), tx1, name, 4);
    fixture.IncrementBlocks(1);

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), name, "two", 2);
    uint160 claimId2 = ClaimIdHash(tx2.GetHash(), 0);
    fixture.IncrementBlocks(1);

    pclaimTrie->getClaimById(claimId2, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId2);


    CMutableTransaction u1 = fixture.MakeUpdate(tx1, name, "updated one", claimId, 1);
    fixture.IncrementBlocks(1);
    pclaimTrie->getClaimById(claimId, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId);
    BOOST_CHECK(claimValue.nAmount == 1);
    BOOST_CHECK(claimValue.outPoint.hash == u1.GetHash());

    fixture.Spend(u1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!pclaimTrie->getClaimById(claimId, claimName, claimValue));

    fixture.DecrementBlocks(3);

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
BOOST_AUTO_TEST_CASE(hardfork_claim_test)
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
BOOST_AUTO_TEST_CASE(hardfork_support_test)
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
    BOOST_CHECK(is_best_claim("test", u2));
    fixture.DecrementBlocks(fixture.extendedExpiration - blocks_before_fork);
    fixture.IncrementBlocks(fixture.extendedExpiration - blocks_before_fork - 1);
    BOOST_CHECK(is_best_claim("test",u1));
    BOOST_CHECK(best_claim_effective_amount_equals("test",3));

}

/*
    claim/support expiration for hard fork, but with checks for disk procedures
*/
BOOST_AUTO_TEST_CASE(hardfork_disk_test)
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
    BOOST_CHECK(best_claim_effective_amount_equals("test2", 2));
    fixture.DecrementBlocks(fixture.originalExpiration - height_of_update_before_expiration);
    // increment to extended expiration, should be expired and not one block before
    fixture.IncrementBlocks(fixture.extendedExpiration - height_of_update_before_expiration);
    BOOST_CHECK(!is_best_claim("test2", u2));
    fixture.DecrementBlocks(fixture.extendedExpiration - height_of_update_before_expiration);
    fixture.IncrementBlocks(fixture.extendedExpiration - height_of_update_before_expiration - 1);
    BOOST_CHECK(is_best_claim("test2", u2));
    BOOST_CHECK(best_claim_effective_amount_equals("test2", 1)); // the support expires one block before
}

BOOST_AUTO_TEST_CASE(insert_update_claim_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName1("atest");
    std::string sName2("btest");
    std::string sName3("atest123");
    std::string sValue1("testa");
    std::string sValue2("testb");
    std::string sValue3("123testa");

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);

    CMutableTransaction tx1 = BuildTransaction(fixture.GetCoinbase());
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME
                                         << std::vector<unsigned char>(sName1.begin(), sName1.end())
                                         << std::vector<unsigned char>(sValue1.begin(), sValue1.end()) << OP_2DROP << OP_DROP << OP_TRUE;
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, tx1.vout[0].nValue - 10001);
    uint160 tx7ClaimId = ClaimIdHash(tx7.GetHash(), 0);
    COutPoint tx7OutPoint(tx7.GetHash(), 0);

    CClaimValue val;
    int nThrowaway;

    // Verify claims (tx7) for uncontrolled names go in immediately
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx7OutPoint);

    // Verify claims for controlled names are delayed, and that the bigger claim wins when inserted
    fixture.IncrementBlocks(5);
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(5); // 12

    BOOST_CHECK(!pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // Verify updates to the best claim get inserted immediately, and others don't.
    CMutableTransaction tx3 = fixture.MakeUpdate(tx1, sName1, sValue1, tx1ClaimId, tx1.vout[0].nValue - 10000);
    COutPoint tx3OutPoint(tx3.GetHash(), 0);
    CMutableTransaction tx9 = fixture.MakeUpdate(tx7, sName1, sValue2, tx7ClaimId, tx7.vout[0].nValue - 10000);
    COutPoint tx9OutPoint(tx9.GetHash(), 0);
    fixture.IncrementBlocks(1, true); // 14

    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx1OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx7OutPoint));
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName1, tx9OutPoint, nThrowaway));

    // Disconnect all blocks until the first block, and then reattach them, in memory only

    CCoinsViewCache coins(pcoinsTip);
    CClaimTrieCache trieCache(pclaimTrie);
    CBlockIndex* pindexState = chainActive.Tip();
    CValidationState state;
    CBlockIndex* pindex;
    for (pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev) {
        CBlock block;
        BOOST_CHECK(ReadBlockFromDisk(block, pindex, Params().GetConsensus()));
        if (pindex == pindexState && (coins.DynamicMemoryUsage() + pcoinsTip->DynamicMemoryUsage()) <= nCoinCacheUsage) {
            bool fClean = true;
            BOOST_CHECK(DisconnectBlock(block, state, pindex, coins, trieCache, &fClean));
            pindexState = pindex->pprev;
        }
    }
    while (pindex != chainActive.Tip()) {
        pindex = chainActive.Next(pindex);
        CBlock block;
        BOOST_CHECK(ReadBlockFromDisk(block, pindex, Params().GetConsensus()));
        BOOST_CHECK(ConnectBlock(block, state, pindex, coins, trieCache));
    }

    // Roll back the last block, make sure tx1 and tx7 are put back in the trie
    fixture.DecrementBlocks(); // 13

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaim(sName1, tx7OutPoint));
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->empty());

    // Roll all the way back, make sure all txs are out of the trie
    fixture.DecrementBlocks();

    BOOST_CHECK(!pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test undoing a claim before the claim gets into the trie

    // Put tx1 in the chain, then advance a few blocks.
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(10); // 11

    // Put tx7 in the chain, verify it goes into the queue
    fixture.CommitTx(tx7);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // Undo that block and make sure it's not in the queue
    fixture.DecrementBlocks();

    // Make sure it's not in the queue
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Go back to the beginning
    fixture.DecrementBlocks();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test spend a claim which was just inserted into the trie

    // Immediately spend tx2 with tx4, verify nothing gets put in the trie
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName2, sValue2, tx1.vout[0].nValue - 1);
    COutPoint tx2OutPoint(tx2.GetHash(), 0);
    CMutableTransaction tx4 = fixture.Spend(tx2);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    fixture.DecrementBlocks(1);
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Verify that if a claim in the queue is spent, it does not get into the trie

    // Put tx5 into the chain, advance until it's in the trie for a few blocks

    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), sName2, sValue2);
    COutPoint tx5OutPoint(tx5.GetHash(), 0);
    fixture.IncrementBlocks(6, true);

    // Put tx2 into the chain, and then advance a few blocks but not far enough for it to get into the trie
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName2, tx2OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(3);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(5);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName2, tx2OutPoint));

    // Undo spending tx2 with tx4, and then advance and verify tx2 is inserted into the trie when it should be

    fixture.DecrementBlocks();
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName2, tx2OutPoint, nThrowaway));

    fixture.IncrementBlocks(2);

    BOOST_CHECK(pclaimTrie->haveClaim(sName2, tx2OutPoint));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.outPoint == tx5OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaim(sName2, tx2OutPoint));

    // Test undoing a spend which involves a claim in the trie

    // spend tx2, which is in the trie, with tx4
    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName2, tx2OutPoint));

    // undo spending tx2 with tx4, and verify tx2 is back in the trie
    fixture.DecrementBlocks();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.outPoint == tx5OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaim(sName2, tx2OutPoint));

    // roll back to the beginning
    fixture.DecrementBlocks();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test undoing a spent update which updated a claim still in the queue

    // Create the claim that will cause the others to be in the queue

    fixture.CommitTx(tx7);
    fixture.IncrementBlocks(6, true);

    // Create the original claim (tx1)

    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName1, tx1OutPoint, nThrowaway));

    // move forward some, but not far enough for the claim to get into the trie
    fixture.IncrementBlocks(2);

    // update the original claim (tx3 spends tx1)
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx1OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx1OutPoint));
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName1, tx3OutPoint, nThrowaway));

    // spend the update (tx6 spends tx3)

    CMutableTransaction tx6 = fixture.Spend(tx3);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx3OutPoint));

    // undo spending the update (undo tx6 spending tx3)
    fixture.DecrementBlocks();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // make sure the update (tx3) still goes into effect when it's supposed to
    fixture.IncrementBlocks(9);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);

    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->haveClaim(sName1, tx3OutPoint));

    // roll all the way back
    fixture.DecrementBlocks();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test undoing an spent update which updated the best claim to a name

    // move forward until the original claim is inserted into the trie

    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(5);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // update the original claim (tx3 spends tx1)
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);

    // spend the update (tx6 spends tx3)
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // undo spending the update (undo tx6 spending tx3)
    fixture.DecrementBlocks();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);

    // Test having two updates to a claim in the same transaction

    // Add both txouts (tx8 spends tx3)
    CMutableTransaction tx8 = BuildTransaction(tx3, 0, 2);
    tx8.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM
                                         << std::vector<unsigned char>(sName1.begin(), sName1.end())
                                         << std::vector<unsigned char>(tx1ClaimId.begin(), tx1ClaimId.end())
                                         << std::vector<unsigned char>(sValue1.begin(), sValue1.end()) << OP_2DROP << OP_2DROP << OP_TRUE;
    tx8.vout[0].nValue = tx8.vout[0].nValue - 1;
    tx8.vout[1].scriptPubKey = CScript() << OP_CLAIM_NAME
                                         << std::vector<unsigned char>(sName1.begin(), sName1.end())
                                         << std::vector<unsigned char>(sValue2.begin(), sValue2.end()) << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx8OutPoint0(tx8.GetHash(), 0);
    COutPoint tx8OutPoint1(tx8.GetHash(), 1);

    fixture.CommitTx(tx8);
    fixture.IncrementBlocks(1, true);

    // ensure txout 0 made it into the trie and txout 1 did not

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx8OutPoint0);

    // roll forward until tx8 output 1 gets into the trie
    fixture.IncrementBlocks(6);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(1);

    // ensure txout 1 made it into the trie and is now in control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx8OutPoint1);

    // roll back to before tx8
    fixture.DecrementBlocks();

    // roll all the way back
    fixture.DecrementBlocks();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // make sure invalid updates don't wreak any havoc

    // put tx1 into the trie
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // advance a few blocks
    fixture.IncrementBlocks(5);

    // put in bad tx10
    CMutableTransaction root = fixture.GetCoinbase();
    CMutableTransaction tx10 = fixture.MakeUpdate(root, sName1, sValue1, tx1ClaimId, root.vout[0].nValue);
    COutPoint tx10OutPoint(tx10.GetHash(), 0);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll back, make sure nothing bad happens
    fixture.DecrementBlocks();

    // put it back in
    fixture.CommitTx(tx10);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // update it
    CMutableTransaction tx11 = fixture.MakeUpdate(tx10, sName1, sValue1, tx1ClaimId, tx10.vout[0].nValue);
    COutPoint tx11OutPoint(tx11.GetHash(), 0);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(10);

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll back to before the update
    fixture.DecrementBlocks();

    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // make sure tx10 would have gotten into the trie, then run tests again

    fixture.IncrementBlocks(10);

    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // update it
    fixture.CommitTx(tx11);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // make sure tx11 would have gotten into the trie

    fixture.IncrementBlocks(20);

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll all the way back
    fixture.DecrementBlocks();

    // Put tx10 and tx11 in without tx1 in
    fixture.CommitTx(tx10);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // update with tx11
    fixture.CommitTx(tx11);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll back to before tx11
    fixture.DecrementBlocks();

    // spent tx10 with tx12 instead which is not a claim operation of any kind
    CMutableTransaction tx12 = BuildTransaction(tx10);

    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll all the way back
    fixture.DecrementBlocks();

    // make sure all claim for names which exist in the trie but have no
    // values get inserted immediately

    CMutableTransaction tx13 = fixture.MakeClaim(fixture.GetCoinbase(), sName3, sValue3, 1);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll back
    fixture.DecrementBlocks();
}

BOOST_AUTO_TEST_CASE(basic_merkle_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue("testa");

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue, 10);
    fixture.IncrementBlocks(20);
    uint256 tx1MerkleHash = pclaimTrie->getMerkleHash();
    fixture.DecrementBlocks(20);
    BOOST_CHECK(tx1MerkleHash != pclaimTrie->getMerkleHash());
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(20);
    BOOST_CHECK(tx1MerkleHash == pclaimTrie->getMerkleHash());
}

BOOST_AUTO_TEST_CASE(claim_expiration_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue("testa");

    int nThrowaway;

    // set expiration time to 200 blocks after the block is created
    pclaimTrie->setExpirationTime(200);

    // create a claim. verify the expiration event has been scheduled.
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue, 10);
    COutPoint tx1OutPoint(tx1.GetHash(), 0);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.

    fixture.IncrementBlocks(199); // 200

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    fixture.IncrementBlocks(1); // 201

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll forward a bit and then roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    fixture.IncrementBlocks(100); // 301

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    fixture.DecrementBlocks(101); // 200

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.
    fixture.IncrementBlocks(1); // 201

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    fixture.DecrementBlocks(2); // 199

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll back some more.
    fixture.DecrementBlocks(99); // 100

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // spend the claim. verify the expiration event is removed.
    CMutableTransaction tx2 = fixture.Spend(tx1);
    fixture.IncrementBlocks(1); // 101

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back the spend. verify the expiration event is returned.
    fixture.DecrementBlocks(1); // 100

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the event occurs on time.
    fixture.IncrementBlocks(100); // 200

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    fixture.IncrementBlocks(1); // 201

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // spend the expired claim
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 202

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // undo the spend. verify everything remains empty.
    fixture.DecrementBlocks(1); // 201

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    fixture.DecrementBlocks(1); // 200

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // verify the expiration event happens at the right time again
    fixture.IncrementBlocks(1); // 201

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify it gets reinserted and expiration gets scheduled.
    fixture.DecrementBlocks(1); // 200

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll all the way back. verify the claim is removed and the expiration event is removed.
    fixture.DecrementBlocks(); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // Make sure that when a claim expires, a lesser claim for the same name takes over

    CClaimValue val;

    // create one claim for the name
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1, true); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance a little while and insert the second claim
    fixture.IncrementBlocks(4); // 5
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue, 5);
    COutPoint tx3OutPoint(tx3.GetHash(), 0);
    fixture.IncrementBlocks(1); // 6

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until tx3 is valid, ensure tx1 is winning
    fixture.IncrementBlocks(4); // 10

    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName, tx3OutPoint, nThrowaway));

    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    uint256 tx1MerkleHash = pclaimTrie->getMerkleHash();

    // roll back to before tx3 is valid
    fixture.DecrementBlocks(1); // 10

    // advance again until tx is valid
    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(tx1MerkleHash == pclaimTrie->getMerkleHash());

    // advance until the expiration event occurs. verify the expiration event occurs on time.
    fixture.IncrementBlocks(189, true); // 200

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    fixture.IncrementBlocks(1); // 201

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);
    BOOST_CHECK(tx1MerkleHash != pclaimTrie->getMerkleHash());

    // spend tx1
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 202

    // roll back to when tx1 and tx3 are in the trie and tx1 is winning
    fixture.DecrementBlocks(); // 11

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(tx1MerkleHash == pclaimTrie->getMerkleHash());

    // roll all the way back
    fixture.DecrementBlocks();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
}

BOOST_AUTO_TEST_CASE(claimtrienode_serialize_unserialize)
{
    CDataStream ss(SER_DISK, 0);

    uint160 hash160;

    CClaimTrieNode n1;
    CClaimTrieNode n2;
    CClaimValue throwaway;

    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.hash.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.hash.SetHex("a79e8a5b28f7fa5e8836a4b48da9988bdf56ce749f81f413cb754f963a516200");
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    CClaimValue v1(COutPoint(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0), hash160, 50, 0, 100);
    CClaimValue v2(COutPoint(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 1), hash160, 100, 1, 101);

    n1.insertClaim(v1);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.insertClaim(v2);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeClaim(v1.outPoint, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeClaim(v2.outPoint, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);
}

BOOST_AUTO_TEST_CASE(invalid_claimid_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;

    // Verify that supports expire

    // Create a 1 LBC claim (tx1)
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Make sure it gets way in there
    fixture.IncrementBlocks(100); // 101

    // Create a 5 LBC claim (tx2)
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue2, 5);
    fixture.IncrementBlocks(1); // 102

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // Create a tx with a bogus claimId
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    CMutableTransaction tx3 = fixture.MakeUpdate(tx2, sName, sValue2, tx1ClaimId, 4);
    CMutableTransaction tx4 = BuildTransaction(tx3);
    COutPoint tx4OutPoint(tx4.GetHash(), 0);

    fixture.IncrementBlocks(1); // 103

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Verify it's not in the claim trie
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    BOOST_CHECK(!pclaimTrie->haveClaim(sName, tx4OutPoint));

    // Update the tx with the bogus claimId
    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1); // 104

    BOOST_CHECK(pcoinsTip->HaveCoins(tx4.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Verify it's not in the claim trie

    BOOST_CHECK(!pclaimTrie->haveClaim(sName, tx4OutPoint));

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // Go forward a few hundred blocks and verify it's not in there

    fixture.IncrementBlocks(300); // 404

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName, tx4OutPoint));

    // go all the way back
    fixture.DecrementBlocks(404);
    BOOST_CHECK(pclaimTrie->empty());
}


BOOST_AUTO_TEST_CASE(supporting_claims_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    int initialHeight = chainActive.Height();

    CClaimValue val;

    // Test 1: create 1 LBC claim (tx1), create 5 LBC support (tx3), create 5 LBC claim (tx2)
    // Verify that tx1 retains control throughout
    // spend tx3, verify that tx2 gains control
    // roll back to before tx3 is spent, verify tx1 regains control
    // update tx1 with tx7, verify tx7 has control
    // roll back to before tx7 is inserted, verify tx1 regains control
    // roll back to before tx2 is valid, spend tx3
    // advance to tx2 valid, verify tx2 gains control
    // roll back to before tx3 is valid, spend tx3
    // advance to tx2 valid, verify tx2 gains control
    // roll back to insertion of tx3, and don't insert it
    // insert tx2, advance until it is valid, verify tx2 gains control

    // Put tx1 in the blockchain

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance a few blocks
    fixture.IncrementBlocks(5); // 6

    // Put tx3 into the blockchain
    CMutableTransaction tx3 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName, 5);
    fixture.IncrementBlocks(1); // 7

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance a few blocks
    fixture.IncrementBlocks(3); // 10

    // Put tx2 into the blockchain
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue2, 5);
    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx2 is valid
    fixture.IncrementBlocks(9); // 20

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(1); // 21

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    BOOST_CHECK(val.outPoint.n == 0);
    uint256 tx1MerkleHash = pclaimTrie->getMerkleHash();

    CMutableTransaction tx4 = BuildTransaction(tx1);
    CMutableTransaction tx5 = BuildTransaction(tx2);
    CMutableTransaction tx6 = BuildTransaction(tx3);

    // spend tx3
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 22

    // verify tx2 gains control
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != pclaimTrie->getMerkleHash());

    // unspend tx3, verify tx1 regains control
    fixture.DecrementBlocks(1); // 21

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    BOOST_CHECK(tx1MerkleHash == pclaimTrie->getMerkleHash());

    // update tx1 with tx7, verify tx7 has control
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    CMutableTransaction tx7 = fixture.MakeUpdate(tx1, sName, sValue1, claimId, tx1.vout[0].nValue);
    fixture.IncrementBlocks(1); // 22

    BOOST_CHECK(pcoinsTip->HaveCoins(tx7.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx7.GetHash());
    BOOST_CHECK(tx1MerkleHash != pclaimTrie->getMerkleHash());

    // roll back to before tx7 is inserted, verify tx1 has control
    fixture.DecrementBlocks(1); // 21

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    BOOST_CHECK(tx1MerkleHash == pclaimTrie->getMerkleHash());

    // roll back to before tx2 is valid
    fixture.DecrementBlocks(1); // 20

    // spend tx3
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 21

    // Verify tx2 gains control
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != pclaimTrie->getMerkleHash());

    // roll back to before tx3 is inserted
    fixture.DecrementBlocks(15); // 6

    // advance a few blocks
    fixture.IncrementBlocks(4); // 10

    // Put tx2 into the blockchain
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx2 is valid
    fixture.IncrementBlocks(9); // 20

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    fixture.IncrementBlocks(2); // 22

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != pclaimTrie->getMerkleHash());

    // roll all the way back
    fixture.DecrementBlocks(22); // 0
    BOOST_CHECK(initialHeight == chainActive.Height());

    // Make sure that when a support in the queue gets spent and then the spend is
    // undone, it goes back into the queue in the right spot

    // put tx2 and tx1 into the trie

    fixture.CommitTx(tx1);
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != pclaimTrie->getMerkleHash());

    // advance a few blocks
    fixture.IncrementBlocks(5); // 6

    // put tx3 into the support queue
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 7

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance a couple of blocks
    fixture.IncrementBlocks(2); // 9

    // spend tx3
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 10

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // undo spend of tx3, verify it gets back in the right place in the queue
    fixture.DecrementBlocks(1); // 9

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    fixture.IncrementBlocks(4); // 13

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    BOOST_CHECK(val.outPoint.n == 0);
    // tx1MerkleHash doesn't match right here because it is at a different activation height (part of the node hash)
    tx1MerkleHash = pclaimTrie->getMerkleHash();

    // spend tx3 again, then undo the spend and roll back until it's back in the queue
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 14

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != pclaimTrie->getMerkleHash());

    fixture.DecrementBlocks(1); // 13

    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    BOOST_CHECK(tx1MerkleHash == pclaimTrie->getMerkleHash());

    // roll all the way back
    fixture.DecrementBlocks(13);
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
}

BOOST_AUTO_TEST_CASE(supporting_claims2_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    CClaimValue val;
    int nThrowaway;

    // Test 2: create 1 LBC claim (tx1), create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 loses control when tx2 becomes valid, and then tx1 gains control when tx3 becomes valid
    // Then, verify that tx2 regains control when A) tx3 is spent and B) tx3 is undone

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance a few blocks
    fixture.IncrementBlocks(4); // 5

    // put tx2 into the blockchain
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue2, 5);
    fixture.IncrementBlocks(1); // 6

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is in the trie
    fixture.IncrementBlocks(4); // 10

    BOOST_CHECK(!pclaimTrie->queueEmpty());
    COutPoint tx2cp(tx2.GetHash(), 0);
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName, tx2cp, nThrowaway));

    fixture.IncrementBlocks(1); // 11
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());

    // advance a few blocks
    fixture.IncrementBlocks(4); // 15

    // put tx3 into the blockchain

    CMutableTransaction tx3 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName, 5);
    fixture.IncrementBlocks(1); // 16

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance until tx3 is valid
    fixture.IncrementBlocks(4); // 20

    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    fixture.IncrementBlocks(1); // 21

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    CMutableTransaction tx4 = BuildTransaction(tx1);
    CMutableTransaction tx5 = BuildTransaction(tx2);
    CMutableTransaction tx6 = BuildTransaction(tx3);

    // spend tx3
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 22

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());

    // undo spend
    fixture.DecrementBlocks(1); // 21

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // roll back to before tx3 is valid
    fixture.DecrementBlocks(1); // 20

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());

    // roll all the way back
    fixture.DecrementBlocks(20); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 4: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 retains control throughout

    // put tx1 into the blockchain

    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // advance a few blocks
    fixture.IncrementBlocks(5); // 6

    // put tx2 into the blockchain
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 7

    uint256 rootMerkleHash = pclaimTrie->getMerkleHash();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    BOOST_CHECK(rootMerkleHash == pclaimTrie->getMerkleHash());

    // advance some, insert tx3, should be immediately valid
    fixture.IncrementBlocks(2); // 9
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 10

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    BOOST_CHECK(rootMerkleHash == pclaimTrie->getMerkleHash());

    // advance until tx2 is valid, verify tx1 retains control
    fixture.IncrementBlocks(3); // 13

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    BOOST_CHECK(rootMerkleHash == pclaimTrie->getMerkleHash());
    BOOST_CHECK(pclaimTrie->haveClaim(sName, tx2cp));

    // roll all the way back
    fixture.DecrementBlocks(13); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 5: create 5 LBC claim (tx2), wait till valid, create 1 LBC claim (tx1), create 5 LBC support (tx3)
    // Verify that tx2 retains control until support becomes valid

    // insert tx2 into the blockchain
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(rootMerkleHash != pclaimTrie->getMerkleHash());

    // advance a few blocks
    fixture.IncrementBlocks(9); // 10

    // insert tx1 into the block chain
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(rootMerkleHash != pclaimTrie->getMerkleHash());

    // advance some
    fixture.IncrementBlocks(5); // 16

    // insert tx3 into the block chain
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 17

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(rootMerkleHash != pclaimTrie->getMerkleHash());

    // advance until tx1 is valid
    fixture.IncrementBlocks(5); // 22

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    COutPoint tx1cp(tx1.GetHash(), 0);
    BOOST_CHECK(pclaimTrie->haveClaim(sName, tx1cp));

    // advance until tx3 is valid
    fixture.IncrementBlocks(11); // 33

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // roll all the way back
    fixture.DecrementBlocks(33); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());


    // Test 6: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3), spend tx1
    // Verify that tx1 retains control until it is spent
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // advance a few blocks
    fixture.IncrementBlocks(5); // 6

    // insert tx2 into the blockchain
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 7

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // advance some, insert tx3
    fixture.IncrementBlocks(2); // 9
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 10

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // advance until tx2 is valid
    fixture.IncrementBlocks(3); // 13

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // spend tx1
    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1); // 14

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());

    // undo spend of tx1
    fixture.DecrementBlocks(1); // 13

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());

    // roll all the way back
    fixture.DecrementBlocks(13); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 7: create 1 LBC claim (tx1), wait till valid, create 5 LBC support (tx3), spend tx1
    // Verify name trie is empty

    // insert tx1 into blockchain
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // insert tx3 into blockchain
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 2

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // spent tx1
    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1); // 3

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // roll all the way back
    fixture.DecrementBlocks(3);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
}

BOOST_AUTO_TEST_CASE(expiring_supports_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;

    pclaimTrie->setExpirationTime(200);

    // to be active bid must have: a higher block number and current block >= (current height - block number) / 32

    // Verify that supports expire

    // Create a 1 LBC claim (tx1)
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);
    fixture.IncrementBlocks(1); // 1, expires at 201

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Create a 5 LBC support (tx3)
    CMutableTransaction tx3 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName, 5);
    fixture.IncrementBlocks(1); // 2, expires at 202

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Advance some, then insert 5 LBC claim (tx2)
    fixture.IncrementBlocks(49); // 51

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue2, 5);
    fixture.IncrementBlocks(1); // 52, activating in (52 - 2) / 1 = 50block (but not then active because support still holds tx1 up)

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    uint256 rootMerkleHash = pclaimTrie->getMerkleHash();

    // Advance until tx2 is valid
    fixture.IncrementBlocks(50); // 102

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(rootMerkleHash == pclaimTrie->getMerkleHash());

    fixture.IncrementBlocks(1); // 103

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx1.GetHash());
    rootMerkleHash = pclaimTrie->getMerkleHash();

    // Update tx1 so that it expires after tx3 expires
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    CMutableTransaction tx4 = fixture.MakeUpdate(tx1, sName, sValue1, claimId, tx1.vout[0].nValue);

    fixture.IncrementBlocks(1); // 104

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx4.GetHash());
    BOOST_CHECK(rootMerkleHash != pclaimTrie->getMerkleHash());

    // Advance until the support expires
    fixture.IncrementBlocks(97); // 201

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    rootMerkleHash = pclaimTrie->getMerkleHash();

    fixture.IncrementBlocks(1); // 202

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    BOOST_CHECK(rootMerkleHash != pclaimTrie->getMerkleHash());
    rootMerkleHash = pclaimTrie->getMerkleHash();

    // undo the block, make sure control goes back
    fixture.DecrementBlocks(1); // 201

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx4.GetHash());
    BOOST_CHECK(rootMerkleHash != pclaimTrie->getMerkleHash());

    // redo the block, make sure it expires again
    fixture.IncrementBlocks(1); // 202

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());
    rootMerkleHash = pclaimTrie->getMerkleHash();

    // roll back some, spend the support, and make sure nothing unexpected
    // happens at the time the support should have expired

    fixture.DecrementBlocks(49); // 153

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx4.GetHash());
    BOOST_CHECK(rootMerkleHash != pclaimTrie->getMerkleHash());

    fixture.Spend(tx3);
    fixture.IncrementBlocks(1); // 154

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());

    fixture.IncrementBlocks(50); // 204

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());

    //undo the spend, and make sure it still expires on time

    fixture.DecrementBlocks(51); // 153

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx4.GetHash());

    fixture.IncrementBlocks(48); // 201

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx4.GetHash());

    fixture.IncrementBlocks(1); // 202

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint.hash == tx2.GetHash());

    // roll all the way back
    fixture.DecrementBlocks(202);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
}

bool verify_proof(const CClaimTrieProof proof, uint256 rootHash, const std::string& name)
{
    uint256 previousComputedHash;
    std::string computedReverseName;
    bool verifiedValue = false;

    for (std::vector<CClaimTrieProofNode>::const_reverse_iterator itNodes = proof.nodes.rbegin(); itNodes != proof.nodes.rend(); ++itNodes) {
        bool foundChildInChain = false;
        std::vector<unsigned char> vchToHash;
        for (std::vector<std::pair<unsigned char, uint256> >::const_iterator itChildren = itNodes->children.begin(); itChildren != itNodes->children.end(); ++itChildren) {
            vchToHash.push_back(itChildren->first);
            uint256 childHash;
            if (itChildren->second.IsNull()) {
                if (previousComputedHash.IsNull()) {
                    return false;
                }
                if (foundChildInChain) {
                    return false;
                }
                foundChildInChain = true;
                computedReverseName += itChildren->first;
                childHash = previousComputedHash;
            } else {
                childHash = itChildren->second;
            }
            vchToHash.insert(vchToHash.end(), childHash.begin(), childHash.end());
        }
        if (itNodes != proof.nodes.rbegin() && !foundChildInChain) {
            return false;
        }
        if (itNodes->hasValue) {
            uint256 valHash;
            if (itNodes->valHash.IsNull()) {
                if (itNodes != proof.nodes.rbegin()) {
                    return false;
                }
                if (!proof.hasValue) {
                    return false;
                }
                valHash = getValueHash(proof.outPoint,
                    proof.nHeightOfLastTakeover);

                verifiedValue = true;
            } else {
                valHash = itNodes->valHash;
            }
            vchToHash.insert(vchToHash.end(), valHash.begin(), valHash.end());
        } else if (proof.hasValue && itNodes == proof.nodes.rbegin()) {
            return false;
        }
        CHash256 hasher;
        std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
        hasher.Write(vchToHash.data(), vchToHash.size());
        hasher.Finalize(&(vchHash[0]));
        uint256 calculatedHash(vchHash);
        previousComputedHash = calculatedHash;
    }
    if (previousComputedHash != rootHash) {
        return false;
    }
    if (proof.hasValue && !verifiedValue) {
        return false;
    }
    std::string::reverse_iterator itComputedName = computedReverseName.rbegin();
    std::string::const_iterator itName = name.begin();
    for (; itName != name.end() && itComputedName != computedReverseName.rend(); ++itName, ++itComputedName) {
        if (*itName != *itComputedName) {
            return false;
        }
    }
    return (!proof.hasValue || itName == name.end());
}

BOOST_AUTO_TEST_CASE(value_proof_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName1("a");
    std::string sValue1("testa");

    std::string sName2("abc");
    std::string sValue2("testabc");

    std::string sName3("abd");
    std::string sValue3("testabd");

    std::string sName4("zyx");
    std::string sValue4("testzyx");

    std::string sName5("zyxa");
    std::string sName6("omg");
    std::string sName7("");

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1);
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName2, sValue2);
    COutPoint tx2OutPoint(tx2.GetHash(), 0);

    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), sName3, sValue3);
    COutPoint tx3OutPoint(tx3.GetHash(), 0);

    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), sName4, sValue4);
    COutPoint tx4OutPoint(tx4.GetHash(), 0);
    CClaimValue val;

    // create a claim. verify the expiration event has been scheduled.
    fixture.IncrementBlocks(5, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);
    BOOST_CHECK(pclaimTrie->getInfoForName(sName3, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);
    BOOST_CHECK(pclaimTrie->getInfoForName(sName4, val));
    BOOST_CHECK(val.outPoint == tx4OutPoint);

    CClaimTrieCache cache(pclaimTrie);

    CClaimTrieProof proof;

    BOOST_CHECK(cache.getProofForName(sName1, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName1));
    BOOST_CHECK(proof.outPoint == tx1OutPoint);

    BOOST_CHECK(cache.getProofForName(sName2, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName2));
    BOOST_CHECK(proof.outPoint == tx2OutPoint);

    BOOST_CHECK(cache.getProofForName(sName3, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName3));
    BOOST_CHECK(proof.outPoint == tx3OutPoint);

    BOOST_CHECK(cache.getProofForName(sName4, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName4));
    BOOST_CHECK(proof.outPoint == tx4OutPoint);

    BOOST_CHECK(cache.getProofForName(sName5, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName5));
    BOOST_CHECK(proof.hasValue == false);

    BOOST_CHECK(cache.getProofForName(sName6, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName6));
    BOOST_CHECK(proof.hasValue == false);

    BOOST_CHECK(cache.getProofForName(sName7, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName7));
    BOOST_CHECK(proof.hasValue == false);

    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), sName7, sValue4);
    COutPoint tx5OutPoint(tx5.GetHash(), 0);

    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->getInfoForName(sName7, val));
    BOOST_CHECK(val.outPoint == tx5OutPoint);

    cache = CClaimTrieCache(pclaimTrie);

    BOOST_CHECK(cache.getProofForName(sName1, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName1));
    BOOST_CHECK(proof.outPoint == tx1OutPoint);

    BOOST_CHECK(cache.getProofForName(sName2, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName2));
    BOOST_CHECK(proof.outPoint == tx2OutPoint);

    BOOST_CHECK(cache.getProofForName(sName3, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName3));
    BOOST_CHECK(proof.outPoint == tx3OutPoint);

    BOOST_CHECK(cache.getProofForName(sName4, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName4));
    BOOST_CHECK(proof.outPoint == tx4OutPoint);

    BOOST_CHECK(cache.getProofForName(sName5, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName5));
    BOOST_CHECK(proof.hasValue == false);

    BOOST_CHECK(cache.getProofForName(sName6, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName6));
    BOOST_CHECK(proof.hasValue == false);

    BOOST_CHECK(cache.getProofForName(sName7, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName7));
    BOOST_CHECK(proof.outPoint == tx5OutPoint);

    fixture.DecrementBlocks();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
}

// Check that blocks with bogus calimtrie hash is rejected
BOOST_AUTO_TEST_CASE(bogus_claimtrie_hash_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName("test");
    std::string sValue1("test");

    int orig_chain_height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);

    CBlockTemplate* pblockTemp;
    BOOST_CHECK(pblockTemp = CreateNewBlock(Params(), tx1.vout[0].scriptPubKey));
    pblockTemp->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    pblockTemp->block.nVersion = 1;
    pblockTemp->block.nTime = chainActive.Tip()->GetBlockTime() + Params().GetConsensus().nPowTargetSpacing;
    CMutableTransaction txCoinbase(pblockTemp->block.vtx[0]);
    txCoinbase.vin[0].scriptSig = CScript() << CScriptNum(chainActive.Height());
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
    pblockTemp->block.vtx[0] = CTransaction(txCoinbase);
    pblockTemp->block.hashMerkleRoot = BlockMerkleRoot(pblockTemp->block);
    //create bogus hash

    uint256 bogusHashClaimTrie;
    bogusHashClaimTrie.SetHex("aaa");
    pblockTemp->block.hashClaimTrie = bogusHashClaimTrie;

    for (uint32_t i = 0;; ++i) {
        pblockTemp->block.nNonce = i;
        if (CheckProofOfWork(pblockTemp->block.GetPoWHash(), pblockTemp->block.nBits, Params().GetConsensus())) {
            break;
        }
    }
    CValidationState state;
    bool success = ProcessNewBlock(state, Params(), NULL, &pblockTemp->block, true, NULL);
    // will process , but will not be connected
    BOOST_CHECK(success);
    BOOST_CHECK(state.IsValid());
    BOOST_CHECK(pblockTemp->block.GetHash() != chainActive.Tip()->GetBlockHash());
    BOOST_CHECK_EQUAL(orig_chain_height, chainActive.Height());
    delete pblockTemp;
}

/*
 * tests for CClaimTrie::getClaimById basic consistency checks
 */
BOOST_AUTO_TEST_CASE(get_claim_by_id_test_2)
{
    ClaimTrieChainFixture fixture;
    const std::string name = "test";
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    CMutableTransaction txx = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);
    uint160 claimIdx = ClaimIdHash(txx.GetHash(), 0);

    fixture.IncrementBlocks(1);

    CClaimValue claimValue;
    std::string claimName;
    pclaimTrie->getClaimById(claimId, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId);

    CMutableTransaction txa = fixture.Spend(tx1);
    CMutableTransaction txb = fixture.Spend(txx);

    fixture.IncrementBlocks(1);
    BOOST_CHECK(!pclaimTrie->getClaimById(claimId, claimName, claimValue));
    BOOST_CHECK(!pclaimTrie->getClaimById(claimIdx, claimName, claimValue));

    fixture.DecrementBlocks(1);
    pclaimTrie->getClaimById(claimId, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId);

    // this test fails
    pclaimTrie->getClaimById(claimIdx, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimIdx);
}

BOOST_AUTO_TEST_CASE(get_claim_by_id_test_3)
{
    ClaimTrieChainFixture fixture;
    pclaimTrie->setExpirationTime(5);
    const std::string name = "test";
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);

    fixture.IncrementBlocks(1);

    CClaimValue claimValue;
    std::string claimName;
    pclaimTrie->getClaimById(claimId, claimName, claimValue);
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId);
    // make second claim with activation delay 1
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 2);
    uint160 claimId2 = ClaimIdHash(tx2.GetHash(), 0);

    fixture.IncrementBlocks(1);
    // second claim is not activated yet, but can still access by claim id
    BOOST_CHECK(is_best_claim(name, tx1));
    BOOST_CHECK(pclaimTrie->getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId2);

    fixture.IncrementBlocks(1);
    // second claim has activated
    BOOST_CHECK(is_best_claim(name, tx2));
    BOOST_CHECK(pclaimTrie->getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId2);


    fixture.DecrementBlocks(1);
    // second claim has been deactivated via decrement
    // should still be accesible via claim id
    BOOST_CHECK(is_best_claim(name, tx1));
    BOOST_CHECK(pclaimTrie->getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId2);

    fixture.IncrementBlocks(1);
    // second claim should have been re activated via increment
    BOOST_CHECK(is_best_claim(name, tx2));
    BOOST_CHECK(pclaimTrie->getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK(claimName == name);
    BOOST_CHECK(claimValue.claimId == claimId2);
}

BOOST_AUTO_TEST_CASE(getclaimsintrie_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("test");
    std::string sValue1("test");
    std::string sName2("test2");
    std::string sValue2("test2");

    fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 42);
    fixture.IncrementBlocks(1);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    fixture.MakeClaim(fixture.GetCoinbase(), sName2, sValue2, 43);
    fixture.IncrementBlocks(1);

    rpcfn_type getclaimsintrie = tableRPC["getclaimsintrie"]->actor;
    UniValue params(UniValue::VARR);

    UniValue results = getclaimsintrie(params, false);
    BOOST_CHECK(results.size() == 2U);
    BOOST_CHECK(results[0]["name"].get_str() == sName1);
    BOOST_CHECK(results[1]["name"].get_str() == sName2);

    params.push_back(blockHash.GetHex());

    results = getclaimsintrie(params, false);
    BOOST_CHECK(results.size() == 1U);
    BOOST_CHECK(results[0]["name"].get_str() == sName1);
}

BOOST_AUTO_TEST_CASE(getclaimtrie_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("test");
    std::string sValue1("test");
    std::string sName2("test2");
    std::string sValue2("test2");

    int height = chainActive.Height();

    fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 42);
    fixture.IncrementBlocks(1);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    fixture.MakeClaim(fixture.GetCoinbase(), sName2, sValue2, 43);
    fixture.IncrementBlocks(1);

    rpcfn_type getclaimtrie = tableRPC["getclaimtrie"]->actor;
    UniValue params(UniValue::VARR);

    UniValue results = getclaimtrie(params, false);
    BOOST_CHECK(results.size() == 6U);
    BOOST_CHECK(results[4]["name"].get_str() == sName1);
    BOOST_CHECK(results[5]["name"].get_str() == sName2);
    BOOST_CHECK(results[4]["height"].get_int() == height + 1);
    BOOST_CHECK(results[5]["height"].get_int() == height + 2);

    params.push_back(blockHash.GetHex());

    results = getclaimtrie(params, false);
    BOOST_CHECK(results.size() == 5U);
    BOOST_CHECK(results[4]["name"].get_str() == sName1);
    BOOST_CHECK(results[4]["height"].get_int() == height + 1);
}

BOOST_AUTO_TEST_CASE(getvalueforname_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("testN");
    std::string sValue1("testV");

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 2);
    fixture.IncrementBlocks(1);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName1, 3);
    fixture.IncrementBlocks(10);

    rpcfn_type getvalueforname = tableRPC["getvalueforname"]->actor;
    UniValue params(UniValue::VARR);
    params.push_back(UniValue(sName1));

    UniValue results = getvalueforname(params, false);
    BOOST_CHECK(results["value"].get_str() == sValue1);
    BOOST_CHECK(results["amount"].get_int() == 2);
    BOOST_CHECK(results["effective amount"].get_int() == 5);

    params.push_back(blockHash.GetHex());

    results = getvalueforname(params, false);
    BOOST_CHECK(results["amount"].get_int() == 2);
    BOOST_CHECK(results["effective amount"].get_int() == 2);
}

BOOST_AUTO_TEST_CASE(getclaimsforname_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("testN");
    std::string sValue1("test1");
    std::string sValue2("test2");

    int height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 2);
    fixture.IncrementBlocks(1);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, 3);
    fixture.IncrementBlocks(1);

    rpcfn_type getclaimsforname = tableRPC["getclaimsforname"]->actor;
    UniValue params(UniValue::VARR);
    params.push_back(UniValue(sName1));

    UniValue results = getclaimsforname(params, false);
    UniValue claims = results["claims"];
    BOOST_CHECK(claims.size() == 2U);
    BOOST_CHECK(results["nLastTakeoverHeight"].get_int() == height + 1);
    BOOST_CHECK(claims[0]["nEffectiveAmount"].get_int() == 0);
    BOOST_CHECK(claims[1]["nEffectiveAmount"].get_int() == 2);
    BOOST_CHECK(claims[0]["supports"].size() == 0U);
    BOOST_CHECK(claims[1]["supports"].size() == 0U);

    fixture.IncrementBlocks(1);

    results = getclaimsforname(params, false);
    claims = results["claims"];
    BOOST_CHECK(claims.size() == 2U);
    BOOST_CHECK(results["nLastTakeoverHeight"].get_int() == height + 3);
    BOOST_CHECK(claims[0]["nEffectiveAmount"].get_int() == 3);
    BOOST_CHECK(claims[1]["nEffectiveAmount"].get_int() == 2);
    BOOST_CHECK(claims[0]["supports"].size() == 0U);
    BOOST_CHECK(claims[1]["supports"].size() == 0U);

    params.push_back(blockHash.GetHex());

    results = getclaimsforname(params, false);
    claims = results["claims"];
    BOOST_CHECK(claims.size() == 1U);
    BOOST_CHECK(results["nLastTakeoverHeight"].get_int() == height + 1);
    BOOST_CHECK(claims[0]["nEffectiveAmount"].get_int() == 2);
    BOOST_CHECK(claims[0]["supports"].size() == 0U);
}

BOOST_AUTO_TEST_CASE(claim_rpcs_rollback2_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("testN");
    std::string sValue1("test1");
    std::string sValue2("test2");

    int height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 1);
    fixture.IncrementBlocks(2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, 2);
    fixture.IncrementBlocks(3);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    CMutableTransaction tx3 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sValue1, 3);
    fixture.IncrementBlocks(1);

    rpcfn_type getclaimsforname = tableRPC["getclaimsforname"]->actor;
    rpcfn_type getvalueforname = tableRPC["getvalueforname"]->actor;

    UniValue params(UniValue::VARR);
    params.push_back(UniValue(sName1));
    params.push_back(blockHash.GetHex());

    UniValue claimsResults = getclaimsforname(params, false);
    BOOST_CHECK(claimsResults["nLastTakeoverHeight"].get_int() == height + 5);
    BOOST_CHECK(claimsResults["claims"][0]["supports"].size() == 0U);
    BOOST_CHECK(claimsResults["claims"][1]["supports"].size() == 0U);

    UniValue valueResults = getvalueforname(params, false);
    BOOST_CHECK(valueResults["value"].get_str() == sValue2);
    BOOST_CHECK(valueResults["amount"].get_int() == 2);
}

BOOST_AUTO_TEST_CASE(claim_rpcs_rollback3_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("testN");
    std::string sValue1("test1");
    std::string sValue2("test2");

    int height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 3);
    fixture.IncrementBlocks(1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, 2);
    fixture.IncrementBlocks(2);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    CMutableTransaction tx3 = fixture.Spend(tx1); // abandon the claim
    fixture.IncrementBlocks(1);

    rpcfn_type getclaimsforname = tableRPC["getclaimsforname"]->actor;
    rpcfn_type getvalueforname = tableRPC["getvalueforname"]->actor;

    UniValue params(UniValue::VARR);
    params.push_back(UniValue(sName1));
    params.push_back(blockHash.GetHex());

    UniValue claimsResults = getclaimsforname(params, false);
    BOOST_CHECK(claimsResults["nLastTakeoverHeight"].get_int() == height + 1);

    UniValue valueResults = getvalueforname(params, false);
    BOOST_CHECK(valueResults["value"].get_str() == sValue1);
    BOOST_CHECK(valueResults["amount"].get_int() == 3);
}

BOOST_AUTO_TEST_SUITE_END()
