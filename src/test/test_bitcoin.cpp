// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_bitcoin.h>

#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <validation.h>
#include <miner.h>
#include <net_processing.h>
#include <policy/policy.h>
#include <pow.h>
#include <ui_interface.h>
#include <streams.h>
#include <rpc/server.h>
#include <rpc/register.h>
#include <script/sigcache.h>

#include "claimtrie.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_parameters.hpp>

void CConnmanTest::AddNode(CNode& node)
{
    LOCK(g_connman->cs_vNodes);
    g_connman->vNodes.push_back(&node);
}

void CConnmanTest::ClearNodes()
{
    LOCK(g_connman->cs_vNodes);
    for (CNode* node : g_connman->vNodes) {
        delete node;
    }
    g_connman->vNodes.clear();
}

uint256 insecure_rand_seed = GetRandHash();
FastRandomContext insecure_rand_ctx(insecure_rand_seed);

extern void noui_connect();

std::ostream& operator<<(std::ostream& os, const uint256& num)
{
    os << num.ToString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const uint160& num)
{
    os << num.ToString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const COutPoint& point)
{
    os << point.ToString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const CClaimValue& claim)
{
    os << "claim(" << claim.outPoint.ToString()
       << ", " << claim.claimId.ToString()
       << ", " << claim.nAmount
       << ", " << claim.nEffectiveAmount
       << ", " << claim.nHeight
       << ", " << claim.nValidAtHeight << ')';
    return os;
}

std::ostream& operator<<(std::ostream& os, const CSupportValue& support)
{
    os << "support(" << support.outPoint.ToString()
       << ", " << support.supportedClaimId.ToString()
       << ", " << support.nAmount
       << ", " << support.nHeight
       << ", " << support.nValidAtHeight << ')';
    return os;
}

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
    : m_path_root(fs::temp_directory_path() / "test_lbrycrd" / strprintf("%lu_%i", (unsigned long)GetTime(), (int)(InsecureRandRange(1 << 30))))
{
    // for debugging:
    if (boost::unit_test::runtime_config::get<boost::unit_test::log_level>(boost::unit_test::runtime_config::btrt_log_level)
            <= boost::unit_test::log_level::log_messages) {
        g_logger->m_print_to_console = true;
        g_logger->m_log_time_micros = true;
        g_logger->EnableCategory(BCLog::ALL);
    }

    SHA256AutoDetect();
    RandomInit();
    ECC_Start();
    SetupEnvironment();
    SetupNetworking();
    InitSignatureCache();
    InitScriptExecutionCache();
    fCheckBlockIndex = true;
    SelectParams(chainName);
    noui_connect();
}

BasicTestingSetup::~BasicTestingSetup()
{
    fs::remove_all(m_path_root);
    ECC_Stop();
}

fs::path BasicTestingSetup::SetDataDir(const std::string& name)
{
    fs::path ret = m_path_root / name;
    fs::create_directories(ret);
    gArgs.ForceSetArg("-datadir", ret.string());
    return ret;
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    SetDataDir("tempdir");
    const CChainParams& chainparams = Params();
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.

        RegisterAllCoreRPCCommands(tableRPC);
        ClearDatadirCache();

        // We have to run a scheduler thread to prevent ActivateBestChain
        // from blocking due to queue overrun.
        threadGroup.create_thread(boost::bind(&CScheduler::serviceQueue, &scheduler));
        GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

        mempool.setSanityCheck(1.0);
        pblocktree.reset(new CBlockTreeDB(1 << 20, true));
        pcoinsdbview.reset(new CCoinsViewDB(1 << 23, true));
        pcoinsTip.reset(new CCoinsViewCache(pcoinsdbview.get()));
        pclaimTrie = new CClaimTrie(true, 1);
        if (!LoadGenesisBlock(chainparams)) {
            throw std::runtime_error("LoadGenesisBlock failed.");
        }
        {
            CValidationState state;
            if (!ActivateBestChain(state, chainparams)) {
                throw std::runtime_error(strprintf("ActivateBestChain failed. (%s)", FormatStateMessage(state)));
            }
        }
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        g_connman = std::unique_ptr<CConnman>(new CConnman(0x1337, 0x1337)); // Deterministic randomness for tests.
        connman = g_connman.get();
        peerLogic.reset(new PeerLogicValidation(connman, scheduler, /*enable_bip61=*/true));
}

TestingSetup::~TestingSetup()
{
        threadGroup.interrupt_all();
        threadGroup.join_all();
        GetMainSignals().FlushBackgroundCallbacks();
        GetMainSignals().UnregisterBackgroundSignalScheduler();
        g_connman.reset();
        peerLogic.reset();
        UnloadBlockIndex();
        pcoinsTip.reset();
        pcoinsdbview.reset();
        pblocktree.reset();
        delete pclaimTrie;
}

TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // CreateAndProcessBlock() does not support building SegWit blocks, so don't activate in these tests.
    // TODO: fix the code to support SegWit blocks.
    UpdateVersionBitsParameters(Consensus::DEPLOYMENT_SEGWIT, 0, Consensus::BIP9Deployment::NO_TIMEOUT);
    // Generate a 100-block chain:
    coinbaseKey.MakeNewKey(true);
    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < COINBASE_MATURITY; i++)
    {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        m_coinbase_txns.push_back(b.vtx[0]);
    }
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    const CChainParams& chainparams = Params();
    BlockAssembler::Options options;
    options.nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
    options.blockMinFeeRate = CFeeRate(0);
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams, options).CreateNewBlock(scriptPubKey);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    for (const CMutableTransaction& tx : txns)
        block.vtx.push_back(MakeTransactionRef(tx));
    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    {
        LOCK(cs_main);
        unsigned int extraNonce = 0;
        IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);
    }

    while (!CheckProofOfWork(block.GetPoWHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
    ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

    CBlock result = block;
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

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction &tx) {
    return FromTx(MakeTransactionRef(tx));
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransactionRef& tx)
{
    return CTxMemPoolEntry(tx, nFee, nTime, nHeight,
                           spendsCoinbase, sigOpCost, lp);
}

/**
 * WARNING: never use real bitcoin block in lbry cause our block header is bigger
 * @returns a block
 */
CBlock getTestBlock()
{
    static CBlock block;
    if (block.IsNull()) {
        block.nVersion = 5;
        block.hashPrevBlock = uint256S("e8fc54d1e6581fceaaf64cc8afeedb9c4ba1eb2349eaf638f1fc41e331869dee");
        block.hashMerkleRoot = uint256S("25bdc368dd4e66dd75ba42140b3d26412aaf9a11a0a7ef4b6b20d5a299644c11");
        block.hashClaimTrie = uint256S("7bae80d60b09031265f8a9f6282e4b9653764fadfac2c8b4c1f416c972b58814");
        block.nTime = 1545050539;
        block.nBits = 0x1a02cb1b;
        block.nNonce = 3189423909;
        block.vtx.resize(4);
        uint256 prevHash = block.hashPrevBlock;
        for (int i = 0; i < 4; ++i) {
            CMutableTransaction tx;
            tx.nVersion = 5;
            tx.nLockTime = 0;
            tx.vin.resize(1);
            tx.vout.resize(1);
            tx.vin[0].prevout.hash = prevHash;
            tx.vin[0].prevout.n = 0;
            tx.vin[0].scriptSig = CScript();
            tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
            tx.vout[0].scriptPubKey = CScript();
            tx.vout[0].nValue = 0;
            prevHash = tx.GetHash();
            block.vtx[i] = MakeTransactionRef(std::move(tx));
        }
    }
    return block;
}
