// Copyright (c) 2011-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/setup_common.h>

#include <banman.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <init.h>
#include <miner.h>
#include <net.h>
#include <noui.h>
#include <pow.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <script/sigcache.h>
#include <streams.h>
#include <txdb.h>
#include <util/memory.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <util/translation.h>
#include <util/validation.h>
#include <validation.h>
#include <validationinterface.h>

#include <functional>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_parameters.hpp>


const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

FastRandomContext g_insecure_rand_ctx;

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

std::ostream& operator<<(std::ostream& os, const CUint256& num)
{
    os << num.ToString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const CUint160& num)
{
    os << num.ToString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const CTxOutPoint& point)
{
    os << point.ToString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const CClaimValue& claim)
{
    os << claim.ToString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const CSupportValue& support)
{
    os << support.ToString();
    return os;
}

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
    : m_path_root(fs::temp_directory_path() / "test_common_" PACKAGE_NAME / strprintf("%lu_%i", (unsigned long)GetTime(), (int)(InsecureRandRange(1 << 30))))
{
    // for debugging:
    if (boost::unit_test::runtime_config::get<boost::unit_test::log_level>(boost::unit_test::runtime_config::btrt_log_level)
            <= boost::unit_test::log_level::log_messages) {
        g_logger->m_print_to_console = true;
        g_logger->m_log_time_micros = true;
        g_logger->EnableCategory(BCLog::ALL);
        CLogPrint::global().setLogger(g_logger);
    }
    fs::create_directories(m_path_root);
    gArgs.ForceSetArg("-datadir", m_path_root.string());
    ClearDatadirCache();
    SelectParams(chainName);
    gArgs.ForceSetArg("-printtoconsole", "0");
    InitLogging();
    LogInstance().StartLogging();

    SHA256AutoDetect();
    ECC_Start();
    SetupEnvironment();
    SetupNetworking();
    InitSignatureCache();
    InitScriptExecutionCache();
    fCheckBlockIndex = true;
    static bool noui_connected = false;
    if (!noui_connected) {
        noui_connect();
        noui_connected = true;
    }
}

BasicTestingSetup::~BasicTestingSetup()
{
    LogInstance().DisconnectTestLogger();
    fs::remove_all(m_path_root);
    ECC_Stop();
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    const CChainParams& chainparams = Params();
    // Ideally we'd move all the RPC tests to the functional testing framework
    // instead of unit tests, but for now we need these here.
    try {
        RegisterAllCoreRPCCommands(tableRPC);

        // We have to run a scheduler thread to prevent ActivateBestChain
        // from blocking due to queue overrun.
        threadGroup.create_thread(std::bind(&CScheduler::serviceQueue, &scheduler));
        GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

        mempool.setSanityCheck(1.0);
        pblocktree.reset(new CBlockTreeDB(1 << 20, true));
        g_chainstate = MakeUnique<CChainState>();
        ::ChainstateActive().InitCoinsDB(
            /* cache_size_bytes */ 1 << 23, /* in_memory */ true, /* should_wipe */ false);
        auto &consensus = chainparams.GetConsensus();
        pclaimTrie = new CClaimTrie(20000000, true, 0, GetDataDir().string(),
                                    consensus.nNormalizedNameForkHeight,
                                    consensus.nOriginalClaimExpirationTime,
                                    consensus.nExtendedClaimExpirationTime,
                                    consensus.nExtendedClaimExpirationForkHeight,
                                    consensus.nAllClaimsInMerkleForkHeight, 1);
        assert(!::ChainstateActive().CanFlushToDisk());
        ::ChainstateActive().InitCoinsCache();
        assert(::ChainstateActive().CanFlushToDisk());
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
        for (int i = 0; i < nScriptCheckThreads - 1; i++)
            threadGroup.create_thread([i]() { return ThreadScriptCheck(i); });

        g_banman = MakeUnique<BanMan>(GetDataDir() / "banlist.dat", nullptr, DEFAULT_MISBEHAVING_BANTIME);
        g_connman = MakeUnique<CConnman>(0x1337, 0x1337); // Deterministic randomness for tests.
    }
    catch(const std::exception& e) {
        fprintf(stderr, "Error constructing the test framework: %s\n", e.what());
        throw;
    }
}

TestingSetup::~TestingSetup()
{
    threadGroup.interrupt_all();
    threadGroup.join_all();
    GetMainSignals().FlushBackgroundCallbacks();
    GetMainSignals().UnregisterBackgroundSignalScheduler();
    g_connman.reset();
    g_banman.reset();
    UnloadBlockIndex();
    g_chainstate.reset();
    pblocktree.reset();
        delete pclaimTrie;
}

TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // CreateAndProcessBlock() does not support building SegWit blocks, so don't activate in these tests.
    // TODO: fix the code to support SegWit blocks.
    gArgs.ForceSetArg("-segwitheight", "432");
    SelectParams(CBaseChainParams::REGTEST);

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
        IncrementExtraNonce(&block, ::ChainActive().Tip(), extraNonce);
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
