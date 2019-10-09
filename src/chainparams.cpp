// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>

#include <assert.h>

#include <chainparamsseeds.h>

//#define FIND_GENESIS

#define GENESIS_MERKLE_ROOT "b8211c82c3d15bcd78bba57005b86fed515149a53a425eb592c07af99fe559cc"

#define MAINNET_GENESIS_HASH "9c89283ba0f3227f6c03b70216b9f665f0118d5e0fa729cedf4fb34d6a34f463"
#define MAINNET_GENESIS_NONCE 1287

#define TESTNET_GENESIS_HASH "9c89283ba0f3227f6c03b70216b9f665f0118d5e0fa729cedf4fb34d6a34f463"
#define TESTNET_GENESIS_NONCE 1287

#define REGTEST_GENESIS_HASH "6e3fcf1299d4ec5d79c3a4c91d624a4acf9e2e173d95a1a0504f677669687556"
#define REGTEST_GENESIS_NONCE 1

bool CheckProofOfWork2(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return error("CheckProofOfWork(): nBits below minimum work");

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return error("CheckProofOfWork(): hash doesn't match nBits");

    return true;
}

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    //txNew.vout[0].scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    //txNew.vout[0].scriptPubKey = CScript() << ParseHex("0425caecb9fbf6cf50979644e85c11e3ec9007fd477fab9683648c6539e59b59c3a4d9b9c0b552c37eee6476f3e0d8425ac0346fe69ad61628b8c340d42fbfa3fd") << OP_CHECKSIG;
    //txNew.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex("e5ff2d9e3a254622ae493573169c0fa94c82fe4f") << OP_EQUALVERIFY << OP_CHECKSIG;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    genesis.hashClaimTrie = uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");

#ifdef FIND_GENESIS
    while (true)
    {
        genesis.nNonce += 1;
        if (CheckProofOfWork2(genesis.GetPoWHash(), nBits, consensus))
        {
            std::cout << "nonce: " << genesis.nNonce << std::endl;
            std::cout << "hex: " << genesis.GetHash().GetHex() << std::endl;
            std::cout << "pow hash: " << genesis.GetPoWHash().GetHex() << std::endl;
            break;
        }
    }
#endif
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "insert timestamp string";//"The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("345991dbf57bfb014b87006acdfafbfc5fe8292f") << OP_EQUALVERIFY << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.nSubsidyLevelInterval = 1<<5;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.BIP16Exception = uint256S("0x00000000000002dc756eebf4f49723ed8d30cc28a5f108eb94b1ba88ac4f9c22");
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("0xdecb9e2cca03a419fd9cca0cb2b1d5ad11b088f22f8f38556d93ac4358b86c24");
        // FIXME: adjust heights
        consensus.BIP65Height = 200000;
        consensus.BIP66Height = 200000;
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 150; //retarget every block
        consensus.nPowTargetSpacing = 150;
        consensus.nOriginalClaimExpirationTime = 262974;
        consensus.nExtendedClaimExpirationTime = 2102400;
        consensus.nExtendedClaimExpirationForkHeight = 400155;
        consensus.nAllowMinDiffMinHeight = -1;
        consensus.nAllowMinDiffMaxHeight = -1;
        consensus.nNormalizedNameForkHeight = 539940; // targeting 21 March 2019
        consensus.nWitnessForkHeight = 680770; // targeting 11 Dec 2019
        consensus.nAllClaimsInMerkleForkHeight = 658310; // targeting 30 Oct 2019
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of a  half week
        consensus.nMinerConfirmationWindow = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800; // May 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147) -- Unused (see nWitnessForkHeight).
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1547942400; // Jan 20, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1548288000; // Jan 24, 2019

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("00000000000000000000000000000000000000000000024108e3204a44a57a5a"); //621000

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("7899464514d0d8854919e87eb234fd5f0c35d06418bd5fd3c1a8f7092b2a9317"); //620000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xe4;
        pchMessageStart[2] = 0xaa;
        pchMessageStart[3] = 0xf1;
        nDefaultPort = 9246;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1446058291, MAINNET_GENESIS_NONCE, 0x1f00ffff, 1, 400000000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
#ifdef FIND_GENESIS
        std::cout << "hex: " << consensus.hashGenesisBlock.GetHex() << std::endl;
        std::cout << "merkle root: " << genesis.hashMerkleRoot.GetHex() << std::endl;
#else
        assert(consensus.hashGenesisBlock == uint256S(MAINNET_GENESIS_HASH));
        assert(genesis.hashMerkleRoot == uint256S(GENESIS_MERKLE_ROOT));
#endif
        vSeeds.clear();
        vFixedSeeds.clear();

        vSeeds.emplace_back("dnsseed1.lbry.io"); // lbry.io
        vSeeds.emplace_back("dnsseed2.lbry.io"); // lbry.io
        vSeeds.emplace_back("dnsseed3.lbry.io"); // lbry.io

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 0x55);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 0x7a);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1, 0x1c);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        bech32_hrp = "lbc";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                { 4000, uint256S("0xa6bbb48f5343eb9b0287c22f3ea8b29f36cf10794a37f8a925a894d6f4519913") },
            }
        };

        chainTxData = ChainTxData{
            1467272478, 4146, 600.0
            /* // Data from rpc: getchaintxstats 4096 0000000000000000002e63058c023a9a1de233554f28c7b21380b6c9003f36a8 */
            /* /\* nTime    *\/ 1532884444, */
            /* /\* nTxCount *\/ 331282217, */
            /* /\* dTxRate  *\/ 2.4 */
        };

        /* disable fallback fee on mainnet */
        m_fallback_fee_enabled = false;
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.nSubsidyLevelInterval = 1 << 5;
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 100;
        consensus.BIP16Exception = uint256S("0x00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105");
        consensus.BIP34Height = 21111;
        consensus.BIP34Hash = uint256S("0x0000000023b3a96d3484e5abb3755c413e7d41500f8e2a5c3f0dd01299cd8ef8");
        // FIXME: adjust heights
        consensus.BIP65Height = 1200000;
        consensus.BIP66Height = 1200000;
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 150;
        consensus.nPowTargetSpacing = 150;
        consensus.nOriginalClaimExpirationTime = 262974;
        consensus.nExtendedClaimExpirationTime = 2102400;
        consensus.nExtendedClaimExpirationForkHeight = 278160;
        consensus.nAllowMinDiffMinHeight = 277299;
        consensus.nAllowMinDiffMaxHeight = 1100000;
        consensus.nNormalizedNameForkHeight = 993380;   // targeting, 21 Feb 2019
        consensus.nWitnessForkHeight = 1198600;
        consensus.nAllClaimsInMerkleForkHeight = 1198560; // targeting 30 Sep 2019
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1456790400; // March 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147) -- Unused (see nWitnessForkHeight).
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1462060800; // May 1st 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1493596800; // May 1st 2017

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000a0c3931735170");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("9812b0bcb7e889e58d999c897e9eaddb2dab98122ff1cfb238ebeef5351bd48c"); // 1

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xe4;
        pchMessageStart[2] = 0xaa;
        pchMessageStart[3] = 0xe1;
        nDefaultPort = 19246;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1446058291, TESTNET_GENESIS_NONCE, 0x1f00ffff, 1, 400000000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
#ifdef FIND_GENESIS
        std::cout << "testnet genesis hash: " << genesis.GetHash().GetHex() << std::endl;
        std::cout << "testnet merkle hash: " << genesis.hashMerkleRoot.GetHex() << std::endl;
#else
        assert(consensus.hashGenesisBlock == uint256S(TESTNET_GENESIS_HASH));
        assert(genesis.hashMerkleRoot == uint256S(GENESIS_MERKLE_ROOT));
#endif

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("testdnsseed1.lbry.io");
        vSeeds.emplace_back("testdnsseed2.lbry.io");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tlbc";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        checkpointData = {
            {
                {0, uint256S(TESTNET_GENESIS_HASH)},
            }
        };

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 4096 0000000000000037a8cd3e06cd5edbfe9dd1dbcc5dacab279376ef7cfc2b4c75
            /* nTime    */ 1531929919,
            /* nTxCount */ 19438708,
            /* dTxRate  */ 0.626
        };

        /* enable fallback fee on testnet */
        m_fallback_fee_enabled = true;
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = CBaseChainParams::REGTEST;
        consensus.nSubsidyLevelInterval = 1 << 5;
        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = 1000; // BIP34 is needed for validation_block_tests
        consensus.BIP34Hash = uint256();
        // FIXME: update heights and add activation tests
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 1;//14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 1;
        consensus.nOriginalClaimExpirationTime = 500;
        consensus.nExtendedClaimExpirationTime = 600;
        consensus.nExtendedClaimExpirationForkHeight = 800;
        consensus.nAllowMinDiffMinHeight = -1;
        consensus.nAllowMinDiffMaxHeight = -1;
        consensus.nNormalizedNameForkHeight = 250; // SDK depends upon this number
        consensus.nWitnessForkHeight = 150;
        consensus.nAllClaimsInMerkleForkHeight = 350;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xe4;
        pchMessageStart[2] = 0xaa;
        pchMessageStart[3] = 0xd1;
        nDefaultPort = 29246;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1446058291, REGTEST_GENESIS_NONCE, 0x207fffff, 1, 400000000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
#ifdef FIND_GENESIS
        std::cout << "regtest genensis hash: " << genesis.GetHash().GetHex() << std::endl;
        std::cout << "regtest hashmerkleroot: " << genesis.hashMerkleRoot.GetHex() << std::endl;
#else
        assert(consensus.hashGenesisBlock == uint256S(REGTEST_GENESIS_HASH));
        assert(genesis.hashMerkleRoot == uint256S(GENESIS_MERKLE_ROOT));
#endif

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = {
            {
                {0, uint256S(REGTEST_GENESIS_HASH)},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rlbc";

        /* enable fallback fee on regtest */
        m_fallback_fee_enabled = true;
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
