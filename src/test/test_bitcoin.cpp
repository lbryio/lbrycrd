// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "key.h"
#include "main.h"
#include "miner.h"
#include "pubkey.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "rpc/server.h"
#include "rpc/register.h"
#ifdef ENABLE_WALLET
#include "wallet/db.h"
#include "wallet/wallet.h"
#endif

#include "test/testutil.h"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h

extern bool fPrintToConsole;
extern void noui_connect();

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
{
        ECC_Start();
        SetupEnvironment();
        SetupNetworking();
        fPrintToDebugLog = false; // don't want to write to debug.log file
        fCheckBlockIndex = true;
        SelectParams(chainName);
        noui_connect();
}

BasicTestingSetup::~BasicTestingSetup()
{
        ECC_Stop();
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    const CChainParams& chainparams = Params();
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.
        RegisterAllCoreRPCCommands(tableRPC);
#ifdef ENABLE_WALLET
        bitdb.MakeMock();
        RegisterWalletRPCCommands(tableRPC);
#endif
        ClearDatadirCache();
        pathTemp = GetTempPath() / strprintf("test_bitcoin_%lu_%i", (unsigned long)GetTime(), (int)(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        pclaimTrie = new CClaimTrie(true, false, 1);
        InitBlockIndex(chainparams);
#ifdef ENABLE_WALLET
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        RegisterValidationInterface(pwalletMain);
#endif
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
        UnregisterNodeSignals(GetNodeSignals());
        threadGroup.interrupt_all();
        threadGroup.join_all();
#ifdef ENABLE_WALLET
        UnregisterValidationInterface(pwalletMain);
        delete pwalletMain;
        pwalletMain = NULL;
#endif
        UnloadBlockIndex();
        delete pclaimTrie;
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;
#ifdef ENABLE_WALLET
        bitdb.Flush(true);
        bitdb.Reset();
#endif
        boost::filesystem::remove_all(pathTemp);
}

TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // Generate a 100-block chain:
    coinbaseKey.MakeNewKey(true);
    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < COINBASE_MATURITY; i++)
    {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        coinbaseTxns.push_back(b.vtx[0]);
    }
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    const CChainParams& chainparams = Params(CBaseChainParams::REGTEST);
    CBlockTemplate *pblocktemplate = CreateNewBlock(chainparams, scriptPubKey);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    BOOST_FOREACH(const CMutableTransaction& tx, txns)
        block.vtx.push_back(tx);
    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

    while (!CheckProofOfWork(block.GetPoWHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

    CValidationState state;
    ProcessNewBlock(state, chainparams, NULL, &block, true, NULL);

    CBlock result = block;
    delete pblocktemplate;
    return result;
}

TestChain100Setup::~TestChain100Setup()
{
}

RegTestingSetup::RegTestingSetup() : TestingSetup(CBaseChainParams::REGTEST)
{
    minRelayTxFee = CFeeRate(0);
    minFeePerNameClaimChar = 0;
}

RegTestingSetup::~RegTestingSetup()
{
    minRelayTxFee = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
}


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(CMutableTransaction &tx, CTxMemPool *pool) {
    CTransaction txn(tx);
    bool hasNoDependencies = pool ? pool->HasNoInputsOf(tx) : hadNoDependencies;
    // Hack to assume either its completely dependent on other mempool txs or not at all
    CAmount inChainValue = hasNoDependencies ? txn.GetValueOut() : 0;

    return CTxMemPoolEntry(txn, nFee, nTime, dPriority, nHeight,
                           hasNoDependencies, inChainValue, spendsCoinbase, sigOpCount, lp);
}

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

bool ShutdownRequested()
{
  return false;
}
