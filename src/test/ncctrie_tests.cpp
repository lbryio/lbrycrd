// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "primitives/transaction.h"
#include "miner.h"
#include "ncctrie.h"
#include "coins.h"
#include "streams.h"
#include <boost/test/unit_test.hpp>
#include <iostream>
#include "test/test_bitcoin.h"

using namespace std;

const unsigned int insert_nonces[] = {
    62302, 78404, 42509, 88397, 232147, 34120, 48944, 8449, 3855, 99418,
    35007, 36992, 18865, 48021, 117592, 61911, 26614, 26267, 171911, 49917,
    68274, 19360, 48650, 22711, 102612, 73362, 7375, 39379, 413, 123283,
    51264, 50359, 11329, 126833, 56973, 48128, 183377, 122530, 68435, 16223,
    201782, 6345, 169401, 49980, 340128, 21022, 54403, 2304, 57721, 257910,
    31720, 13689, 73758, 43961, 14926, 90259, 23943, 75907, 70683, 91654,
    2788, 110836, 21685, 78041, 3310, 39497, 1774, 51369, 59835, 41272,
    41575, 63281, 25029, 87528, 285, 34225, 48228, 5152, 22230, 83385,
    11314, 192174, 75562, 116667, 385684, 56288, 23660, 18636, 228282, 114723,
    623, 251464, 15557, 88445, 141000, 111823, 3972, 25959, 26559, 137659,
    47864, 70562, 102076, 827, 28810, 29768, 71901, 45824, 81128, 144277,
    23405, 14275, 23837, 2713, 24162, 49329, 115878, 50877, 124383, 144840,
    266179, 121554, 47532, 6247, 28255, 254151, 48955, 84748, 241948, 35275,
    16909, 133791, 93138, 59518, 70972, 120051, 109811, 20584, 17823, 101763,
    25577, 24277, 3482, 42387, 103600, 79949, 93276, 3258, 13991, 10782,
    344259, 77179, 100907, 79587, 9552, 48822, 16915, 73331, 37263, 112357,
    31685, 46691, 155015, 119072, 9543, 2292, 89140, 13762, 29899, 36874,
    122924, 42, 22902, 36627, 4244, 37166, 34686, 120765, 10210, 134614,
    23799, 149425, 120024, 27766, 22997, 31651, 4055, 9770, 67895, 81757,
    13057, 50235, 166795, 22267, 155530, 28458, 135516, 47850, 14820, 54174,
    66691, 196100, 31644, 9822, 72770, 46088, 11563, 124579, 131847, 33348,
    49053, 64349, 258375, 24202, 96640, 42232, 116531, 133425, 50283, 77773,
    21118, 137282, 165647, 83029, 144604, 81135, 67711, 132395, 29024, 140422,
    34761, 56323, 43292, 63797, 5586, 48860, 56797, 101110, 16820, 166363,
    109360, 78408, 223452, 3201, 60430, 21485, 33348, 112307, 2390, 10911,
    25658, 22369, 62658, 82807, 37511, 45794, 19810, 17668, 72189, 369983,
    5117, 208398, 196524, 171, 7371, 24445, 110133, 72669, 89731, 99758,
    29522, 55820, 60793, 31310, 171461, 80660, 123819, 58962, 54153, 53058,
    1729, 62947, 51374, 133045, 4983, 33163, 8340, 13066, 29664, 4474,
    235529, 253826, 80953, 64694, 277894, 19611, 13659, 30347, 31926, 4686,
    80304, 6650, 31670, 174453, 136085, 223321, 15262, 3299, 34516, 137382,
    106996, 68709, 17486, 110837, 157075, 11078, 157597, 11171, 127598, 64125,
    57626, 58654, 44730, 107781, 22635, 94765, 10103, 20723, 7509, 1081,
    24352, 4846, 29187, 453703, 5114, 1244, 626, 133986, 47067, 4149,
    110343, 21613, 33002, 120824, 109164, 51170, 83888, 4209, 2421, 57893,
    164672, 16594, 51217, 118785, 31325, 1017, 64481, 13050, 37412, 169320,
    13569, 26559, 51805, 71218, 236435, 114049, 492, 16652, 40248, 13693,
    21022, 20441, 2176, 188005, 60471, 3688, 3054, 170834, 23534, 19256,
    50302, 30519, 22514, 4447, 85147, 25983, 85002, 20411, 40249, 22878,
    14010, 44647, 34606, 186404, 110909, 36338, 62987, 29983, 7192, 35465,
    2995, 33457, 93500, 24146, 38031, 11953, 66874, 7590, 139803, 9525,
    53444, 254006, 26853, 16617, 35308, 71741, 59383, 37348, 156748, 93838,
    96215, 47298, 35119, 58025, 51044, 145982, 8151, 177835, 6315, 15025,
    2504, 215135, 103144, 2492, 192, 38240, 57417, 11725, 48369, 182146,
    10941, 63185, 160233, 34699, 35012, 158130, 37669, 26041, 36852, 37891,
    9879, 9892, 34605, 25583, 3323, 160175, 6951, 44767, 5039, 62761,
    27795, 179460, 7358, 216, 10407, 257742, 69315, 2617, 134737, 66435,
    56215, 17714, 38908, 47160, 21354, 31881, 42867, 58843, 26370, 11260,
    11813, 212327, 33685, 113596, 105159, 73144, 24356, 97897, 20602, 10452,
    12250, 62390, 38375, 60273, 23336, 33806, 40617, 22276, 60715, 194148,
    50822, 110070, 2102, 6397, 70139, 10539, 8155, 183926, 19102, 34694,
    158006, 73770, 18564, 37019, 12673, 12681, 19243, 35069, 235750, 12110,
    17397, 2317, 213831, 12315, 78126, 116545, 107835, 116366, 57241, 22608,
    79014, 32275, 12060, 55835, 17401, 102239, 14668, 20121, 4119, 30656,
    69942, 66483, 25954, 12711, 121536, 12394, 84011, 12767, 56377, 164783,
    37048, 88712, 58853, 129293, 472, 37208, 368, 45723, 127506, 44908,
    1203, 129491, 153675, 2208, 1315, 120624, 32408, 90338, 99397, 72400,
    4403, 32782, 62787, 11962, 15175, 83275, 24585, 6447, 81927, 4001,
    108118, 48080, 141528, 36406, 9629, 46372, 16180, 48559, 128234, 212518,
    47805, 232605, 245798, 15869, 68322, 4530, 19700, 10275, 120746, 46854,
    54898, 29476, 74410, 247807, 39933, 46868, 56451, 36564, 103828, 41302,
    28903, 11601, 119842, 23196, 219677, 61984, 706, 66649, 56481, 14360,
    128017, 19310, 12892, 3295, 37737, 100578, 26914, 27946, 2172, 212021,
    3100, 79199, 7024, 129311, 43027, 9696, 86251, 9574, 113123, 32433,
    18065, 121598, 5003, 11516, 138320, 46619, 96118, 85037, 14106, 7904,
    102894, 41478, 509, 169951, 5571, 28284, 8138, 53248, 47878, 113791,
    192277, 73645, 28672, 93670, 30741, 129667}; 

const unsigned int expire_nonces[] = {
   1772, 19895, 52494, 75900, 15954, 92687, 381518, 24750, 131218, 140637,
   25151, 4751, 117588, 31281, 741, 13006, 56207, 23174, 32024, 23597,
   17093, 22998, 68975, 84976, 47024, 48818, 29830, 28891, 66091, 6304,
   117, 253782, 126584, 55982, 43172, 5862, 36712, 289031, 42833, 61902,
   81993, 26790, 80145, 111102, 27904, 16188, 55120, 7171, 67694, 214899,
   16360, 133920, 31442, 103537, 266284, 4407, 102008, 1149, 156950, 76029,
   33751, 20831, 24714, 202595, 9393, 227174, 3357, 70, 45445, 104755,
   6024, 13512, 163718, 43755, 17582, 17069, 17355, 38898, 101900, 41342,
   20701, 9827, 11663, 146362, 189082, 279537, 79142, 423101, 11154, 103046,
   44740, 90350, 41450, 4627, 4685, 14510, 13639, 13705, 2369, 326596,
   74814, 15068, 7287, 26685, 41834, 5872, 94033, 77510, 21271, 51233,
   687, 84716, 18945, 139111, 11508, 15009, 103492, 25317, 100121, 29105,
   26520, 531, 6883, 181308, 161707, 72079, 50032, 65286, 96365, 18280,
   21015, 56591, 90913, 3280, 74270, 87610, 85124, 58599, 161342, 36017,
   135104, 19625, 114763, 163177, 31400, 99871, 15939, 6822, 51007, 33240,
   15449, 28232, 242057, 45744, 253284, 218826, 25843, 118582, 228181, 7592,
   3398, 23637, 46762, 22853, 7669, 358599, 99238, 32782, 46407, 36545,
   28976, 34995, 23564, 60624, 45957, 162829, 7055, 184611, 69270, 22560,
   17314, 24374, 67156, 44705, 9482, 17611, 92669, 22471, 47688, 110960,
   18595, 46376, 23989, 47847, 41231, 2937, 34056, 61839, 9393, 122723,
   119967, 41795, 13244, 22081, 41407, 65568, 120355, 228591, 3763, 23561,
   78832, 20110, 3849, 162319, 9699, 153960, 60013, 22063, 22895, 84997,
   20008, 6201, 18985, 7065, 63655, 35849, 19788, 69977, 45664, 15647,
   111735, 1372, 2487, 38856, 39081, 74496, 71062, 20232, 33091, 26777,
   155180, 149434, 35380, 102844, 46709, 3373, 137389, 33273, 13194, 5106,
   69109, 21599, 24896, 16688, 101584, 567, 61253, 125648, 30825, 38976,
   88494, 82214, 84079, 52761, 16392, 206857, 19856, 48376, 11820, 21296,
   12707, 94783, 31272, 113581, 40682, 143247, 1206, 21720, 40627, 131077,
   78794, 9026, 38830, 39757, 94577, 20134, 15904, 185817, 35595, 21609,
   15868, 187480, 62515, 36383, 12797, 101029, 17979, 48892, 59215, 197023,
   217536, 295885, 5639, 24470, 440, 96896, 3113, 74270, 8773, 89871,
   41807, 33896, 41053, 87038, 64666, 2642, 39880, 113389, 51922, 64364,
   46026, 34424, 6856, 51050, 48871, 43033, 95444, 68098, 10463, 27119,
   101386, 28555, 94429, 38046, 13110, 39449, 976, 12015, 32827, 20156,
   79086, 90632, 24716, 11632, 27225, 11563, 45437, 41804, 240056, 26298,
   7089, 119823, 87043, 46315, 108722, 24002, 109328, 32013, 47232, 243845,
   9604, 158119, 189402, 124127, 89656, 39496, 63807, 83836, 80843, 2261,
   103229, 24028, 299032, 43513, 8506, 362389, 14113, 47973, 145654, 23170,
   23272, 35090, 17628, 28197, 96581, 84151, 694, 182728, 86514, 129029,
   4509, 48647, 63280, 86829, 138046, 8075, 59670, 53232, 37353, 51966,
   62110, 66890, 75512, 42296, 240099, 64624, 98903, 29189, 2593, 17376,
   11417, 2982, 3273, 40001, 468410, 29603, 111819, 36878, 6710, 14350,
   87778, 69800, 18820, 75880, 26049, 36685, 8500, 189289, 186944, 100520,
   167630, 7170, 115904, 79212, 4792, 82507, 13634, 71118, 27066, 66914,
   44963, 58999, 153183, 184462, 65596, 102627, 333388, 164930, 6664, 120605,
   141, 122317, 162948, 93160, 216861, 68595, 67370, 15526, 4320, 45961,
   86806, 61077, 61730, 117548, 127472, 1191, 15735, 133987, 16288, 4652,
   83674, 21872, 21351, 35691, 11584, 85669, 196, 75276, 45275, 55828,
   46586, 153482, 6183, 386, 28729, 111942, 2847, 10584, 207349, 57260,
   12075, 27552, 129129, 282325, 11954, 28300, 175155, 31256, 12953, 16345,
   34101, 57962, 16950, 9103, 94414, 1918, 69031, 151629, 41281, 85752,
   66681, 31646, 12396, 167, 72005, 3936, 27566, 5663, 24098, 73350,
   19076, 179631, 36500, 152550, 56501, 81202, 77561, 134713, 81130, 22321,
   112081, 32992, 144573, 21369, 2471, 18371, 63050, 44211, 6147, 206052,
   34252, 534, 20176, 58035, 24268, 19608, 37770, 57588, 120961, 58415,
   4780, 4614, 229320, 42279, 41295, 23501, 78183, 171983, 24229}; 

BOOST_FIXTURE_TEST_SUITE(ncctrie_tests, TestingSetup)

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

CMutableTransaction BuildTransaction(const CMutableTransaction& prev)
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = prev.GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = CScript();
    tx.vout[0].nValue = prev.vout[0].nValue;

    return tx;
}

CMutableTransaction BuildTransaction(const CTransaction& prev)
{
    return BuildTransaction(CMutableTransaction(prev));
}

void AddToMempool(CMutableTransaction& tx)
{
    mempool.addUnchecked(tx.GetHash(), CTxMemPoolEntry(tx, 0, GetTime(), 111.0, chainActive.Height()));
}

bool CreateBlock(CBlockTemplate* pblocktemplate, int nonce)
{
    static int unique_block_counter = 0;
    CBlock* pblock = &pblocktemplate->block;
    pblock->nVersion = 1;
    pblock->nTime = chainActive.Tip()->GetMedianTimePast()+1;
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = CScript() << CScriptNum(unique_block_counter++) << CScriptNum(chainActive.Height());
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height(), Params().GetConsensus());
    pblock->vtx[0] = CTransaction(txCoinbase);
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
    //if (nonce != -1)
    pblock->nNonce = nonce;
    /*else
    {
        for (int i = 0; ; ++i)
        {
            pblock->nNonce = i;
            if (CheckProofOfWork(pblock->GetHash(), pblock->nBits, Params().GetConsensus()))
            {
                std::cout << pblock->nNonce << ",";
                if (unique_block_counter % 10 == 0)
                    std::cout << std::endl;
                else
                    std::cout << " "; 
                break;
            }
        }
    }*/
    CValidationState state;
    bool success = (ProcessNewBlock(state, NULL, pblock) && state.IsValid() && pblock->GetHash() == chainActive.Tip()->GetBlockHash());
    pblock->hashPrevBlock = pblock->GetHash();
    return success;
}

bool RemoveBlock(uint256& blockhash)
{
    CValidationState state;
    if (mapBlockIndex.count(blockhash) == 0)
        return false;
    CBlockIndex* pblockindex = mapBlockIndex[blockhash];
    InvalidateBlock(state, pblockindex);
    if (state.IsValid())
    {
        ActivateBestChain(state);
    }
    return state.IsValid();

}

BOOST_AUTO_TEST_CASE(ncctrie_merkle_hash)
{
    int unused;
    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CMutableTransaction tx1 = BuildTransaction(hash0);
    CMutableTransaction tx2 = BuildTransaction(tx1.GetHash());
    CMutableTransaction tx3 = BuildTransaction(tx2.GetHash());
    CMutableTransaction tx4 = BuildTransaction(tx3.GetHash());
    CMutableTransaction tx5 = BuildTransaction(tx4.GetHash());
    CMutableTransaction tx6 = BuildTransaction(tx5.GetHash());

    uint256 hash1;
    hash1.SetHex("09732c6efebb4065a27f184285a6b66280a978cf972b1f41a563f0d3644f3e5c");
    
    uint256 hash2;
    hash2.SetHex("8db92b76b6b0416d7abda4fd7404ba69e340853dfe9ffc04843c50142b857471");
    
    uint256 hash3;
    hash3.SetHex("2e38561067f3e83d6ca0b627861689de72bba77cb5aca69d13b3685b5229525b");
    
    uint256 hash4;
    hash4.SetHex("6ec84089eb4ca1aeead6cf3538d4def78baebb44715c31be8790e627e0dcbc28");

    BOOST_CHECK(pnccTrie->empty());

    CNCCTrieCache ntState(pnccTrie);
    ntState.insertClaimIntoTrie(std::string("test"), CNodeValue(tx1.GetHash(), 0, 50, 100, 200));
    ntState.insertClaimIntoTrie(std::string("test2"), CNodeValue(tx2.GetHash(), 0, 50, 100, 200));

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!ntState.empty());
    BOOST_CHECK(ntState.getMerkleHash() == hash1);

    ntState.insertClaimIntoTrie(std::string("test"), CNodeValue(tx3.GetHash(), 0, 50, 101, 201));
    BOOST_CHECK(ntState.getMerkleHash() == hash1);
    ntState.insertClaimIntoTrie(std::string("tes"), CNodeValue(tx4.GetHash(), 0, 50, 100, 200));
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.insertClaimIntoTrie(std::string("testtesttesttest"), CNodeValue(tx5.GetHash(), 0, 50, 100, 200));
    ntState.removeClaimFromTrie(std::string("testtesttesttest"), tx5.GetHash(), 0, unused);
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.flush();

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState1(pnccTrie);
    ntState1.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("test2"), tx2.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("tes"), tx4.GetHash(), 0, unused);

    BOOST_CHECK(ntState1.getMerkleHash() == hash0);

    CNCCTrieCache ntState2(pnccTrie);
    ntState2.insertClaimIntoTrie(std::string("abab"), CNodeValue(tx6.GetHash(), 0, 50, 100, 200));
    ntState2.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);

    BOOST_CHECK(ntState2.getMerkleHash() == hash3);

    ntState2.flush();

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash3);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState3(pnccTrie);
    ntState3.insertClaimIntoTrie(std::string("test"), CNodeValue(tx1.GetHash(), 0, 50, 100, 200));
    BOOST_CHECK(ntState3.getMerkleHash() == hash4);
    ntState3.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash4);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState4(pnccTrie);
    ntState4.removeClaimFromTrie(std::string("abab"), tx6.GetHash(), 0, unused);
    BOOST_CHECK(ntState4.getMerkleHash() == hash2);
    ntState4.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState5(pnccTrie);
    ntState5.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);

    BOOST_CHECK(ntState5.getMerkleHash() == hash2);
    ntState5.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState6(pnccTrie);
    ntState6.insertClaimIntoTrie(std::string("test"), CNodeValue(tx3.GetHash(), 0, 50, 101, 201));

    BOOST_CHECK(ntState6.getMerkleHash() == hash2);
    ntState6.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState7(pnccTrie);
    ntState7.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("tes"), tx4.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("test2"), tx2.GetHash(), 0, unused);
    
    BOOST_CHECK(ntState7.getMerkleHash() == hash0);
    ntState7.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pnccTrie->checkConsistency());
}

BOOST_AUTO_TEST_CASE(ncctrie_insert_update_claim)
{
    int block_counter = 0;
    BOOST_CHECK(pnccTrie->nCurrentHeight == chainActive.Height() + 1);
    
    CBlockTemplate *pblocktemplate;
    LOCK(cs_main);
    //Checkpoints::fEnabled = false;

    CScript scriptPubKey = CScript() << OP_TRUE;

    std::string sName1("atest");
    std::string sName2("btest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName1(sName1.begin(), sName1.end());
    std::vector<unsigned char> vchName2(sName2.begin(), sName2.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());
    

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    std::vector<CTransaction> coinbases;

    for (unsigned int i = 0; i < 103; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
        if (coinbases.size() < 3)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }

    delete pblocktemplate;

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);
    
    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx3 = BuildTransaction(tx1);
    tx3.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx4 = BuildTransaction(tx2);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;
    CMutableTransaction tx5 = BuildTransaction(coinbases[2]);
    tx5.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CNodeValue val;

    std::vector<uint256> blocks_to_invalidate;
    
    // Verify updates to the best claim get inserted immediately, and others don't.
    
    // Put tx1 and tx2 into the blockchain, and then advance 100 blocks to put them in the trie
    
    AddToMempool(tx1);
    AddToMempool(tx2);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 3);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    
    BOOST_CHECK(!pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pnccTrie->getInfoForName(sName2, val));
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    delete pblocktemplate;
    
    // Verify tx1 and tx2 are in the trie

    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(pnccTrie->getInfoForName(sName2, val));

    // Spend tx1 with tx3, tx2 with tx4, and put in tx5.
    
    AddToMempool(tx3);
    AddToMempool(tx4);
    AddToMempool(tx5);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 4);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    // Verify tx1, tx2, and tx5 are not in the trie, but tx3 is in the trie.

    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    BOOST_CHECK(!pnccTrie->getInfoForName(sName2, val));

    // Roll back the last block, make sure tx1 and tx2 are put back in the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    BOOST_CHECK(pnccTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Roll all the way back, make sure all txs are out of the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pnccTrie->getInfoForName(sName2, val));
    mempool.clear();
    
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pnccTrie->queueEmpty());

    // Test undoing a claim before the claim gets into the trie

    // Put tx1 in the chain, and then undo that block.
    
    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // Make sure it's not in the queue

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing a claim that has gotten into the trie

    // Put tx1 in the chain, and then advance until it's in the trie

    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    delete pblocktemplate;
       
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // Remove it from the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spend which involves a claim just inserted into the queue

    // Immediately spend tx2 with tx4, verify nothing gets put in the trie

    AddToMempool(tx2);
    AddToMempool(tx4);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 3);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spend which involves a claim inserted into the queue by a previous block
    
    // Put tx2 into the chain, and then advance a few blocks but not far enough for it to get into the trie

    AddToMempool(tx2);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    AddToMempool(tx4);
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 51; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // Undo spending tx2 with tx4, and then advance and verify tx2 is inserted into the trie when it should be

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    mempool.clear();

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    delete pblocktemplate;
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Test undoing a spend which involves a claim in the trie

    // spend tx2, which is in the trie, with tx4
    
    AddToMempool(tx4);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // undo spending tx2 with tx4, and verify tx2 is back in the trie
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // roll back to the beginning
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spent update which updated a claim still in the queue

    // Create the original claim (tx1)

    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // move forward some, but not far enough for the claim to get into the trie

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // update the original claim (tx3 spends tx1)

    AddToMempool(tx3);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // spend the update (tx6 spends tx3)

    AddToMempool(tx6);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // undo spending the update (undo tx6 spending tx3)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    mempool.clear();

    // make sure the update (tx3) still goes into effect in 100 blocks

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // undo updating the original claim (tx3 spends tx1)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing an spent update which updated the best claim to a name

    // move forward until the original claim is inserted into the trie

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // update the original claim (tx3 spends tx1)
    
    AddToMempool(tx3);
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    delete pblocktemplate;
    
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    
    // spend the update (tx6 spends tx3)
    
    AddToMempool(tx6);
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    
    // undo spending the update (undo tx6 spending tx3)
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
}

BOOST_AUTO_TEST_CASE(ncctrie_claim_expiration)
{
    int block_counter = 0;
    BOOST_CHECK(pnccTrie->nCurrentHeight == chainActive.Height() + 1);

    CBlockTemplate *pblocktemplate;
    LOCK(cs_main);

    CScript scriptPubKey = CScript() << OP_TRUE;

    std::string sName("atest");
    std::string sValue("testa");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue(sValue.begin(), sValue.end());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    std::vector<CTransaction> coinbases;
    for (unsigned int i = 0; i < 102; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
        if (coinbases.size() < 2)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }

    delete pblocktemplate;

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(tx1);
    tx2.vout[0].scriptPubKey = CScript() << OP_TRUE;

    std::vector<uint256> blocks_to_invalidate;
    // set expiration time to 100 blocks after the block becomes valid.

    pnccTrie->setExpirationTime(200);

    // create a claim. verify no expiration event has been scheduled.

    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // advance until the claim is valid. verify the expiration event is scheduled.

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());
    
    // roll forward a bit and then roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());
    
    // advance until the expiration event occurs. verify the expiration event occurs on time.
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());
    
    // roll back to before the claim is valid. verify the claim is removed but the expiration event still exists.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // advance until the claim is valid again. verify the expiration event is scheduled.

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
        if (i == 50)
            blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the expiration event is scheduled.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // roll back some.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // spend the claim. verify the expiration event is removed.
    
    AddToMempool(tx2);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());
    
    // roll back the spend. verify the expiration event is returned.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());
   
    mempool.clear();
    
    // advance until the expiration event occurs. verify the event occurs on time.
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 50; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());

    // spend the expired claim

    AddToMempool(tx2);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());

    // undo the spend. verify everything remains empty.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());

    mempool.clear();
    
    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // verify the expiration event happens at the right time again

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify it gets reinserted and expiration gets scheduled.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // roll back to before the claim is valid. verify the claim is removed but the expiration event is not.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->expirationQueueEmpty());
    BOOST_CHECK(blocks_to_invalidate.empty());
}

BOOST_AUTO_TEST_CASE(ncctrienode_serialize_unserialize)
{
    CDataStream ss(SER_DISK, 0);

    CNCCTrieNode n1;
    CNCCTrieNode n2;
    CNodeValue throwaway;
    
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

    CNodeValue v1(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0, 50, 0, 100);
    CNodeValue v2(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 1, 100, 1, 101);

    n1.insertValue(v1);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.insertValue(v2);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeValue(v1.txhash, v1.nOut, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeValue(v2.txhash, v2.nOut, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);
}

BOOST_AUTO_TEST_SUITE_END()
