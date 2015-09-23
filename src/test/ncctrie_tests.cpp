// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "consensus/validation.h"
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
	47864, 70562, 102076, 29568, 7219, 6132, 207855, 27429, 38443, 37445, 
	64206, 135854, 93674, 103500, 67021, 58095, 118236, 91253, 64567, 19366, 
	70102, 666, 80009, 9994, 39820, 3524, 2212, 85667, 47668, 25021, 
	182192, 29285, 3769, 86857, 126797, 49162, 16952, 50760, 120889, 3173, 
	2962, 2436, 58781, 104999, 14241, 61552, 90007, 22316, 95414, 104797, 
	24690, 31606, 14425, 12265, 7724, 40322, 50948, 84037, 90870, 90439, 
	79317, 19064, 25852, 67993, 132666, 93574, 11211, 204244, 3274, 108448, 
	61865, 21722, 15966, 19349, 5051, 37715, 12500, 2933, 55290, 8778, 
	64996, 22325, 18638, 132615, 77471, 59497, 64174, 10540, 34782, 12876, 
	36011, 19461, 122409, 41333, 346439, 12835, 161, 29867, 5664, 24276, 
	110629, 41705, 82403, 117918, 2686, 22781, 74781, 8059, 13574, 80700, 
	259435, 94645, 2441, 79068, 87422, 7437, 1197, 4969, 51038, 17387, 
	132591, 106293, 164375, 100688, 20687, 44270, 32408, 33912, 41316, 27930, 
	18173, 60038, 113697, 127820, 39899, 44525, 55585, 31759, 58538, 3297, 
	437, 27842, 167299, 119639, 32566, 121136, 7388, 71680, 45357, 195629, 
	127616, 55406, 78161, 76184, 333, 72817, 1730, 114948, 10746, 148968, 
	182375, 37126, 105817, 48017, 36250, 44934, 24850, 95678, 104566, 95614, 
	28324, 76120, 5795, 179419, 69318, 152410, 108472, 21062, 215696, 27140, 
	52488, 135070, 23647, 47157, 113538, 13583, 78351, 12363, 18558, 127062, 
	43971, 515, 84045, 87171, 79124, 146078, 9189, 4323, 9875, 343500, 
	99225, 28037, 11630, 28675, 90970, 6716, 27681, 12145, 104170, 67804, 
	112975, 83973, 194635, 20363, 1392, 16861, 64895, 41499, 3914, 94319, 
	73582, 13873, 56605, 32687, 16570, 104036, 7131, 48046, 192954, 132191, 
	17337, 36860, 32876, 4682, 20878, 155049, 545, 2662, 92339, 36864, 
	6169, 442438, 57917, 230552, 18608, 470961, 13556, 99731, 103752, 89833, 
	65375, 108902, 12204, 116092, 11987, 14974, 16281, 19849, 9684, 19262, 
	60274, 3922, 4561, 50643, 70270, 26895, 68064, 41757, 61724, 53073, 
	3175, 5290, 75838, 79923, 20281, 38820, 12629, 9813, 23148, 74146, 
	110888, 61261, 142701, 86139, 132205, 20238, 45403, 16518, 31533, 18313, 
	23873, 8693, 50244, 135146, 144813, 7323, 38316, 9083, 27235, 211421, 
	97467, 212336, 5376, 61095, 120776, 24573, 8260, 230917, 86233, 16242, 
	91065, 123686, 4773, 74425, 60920, 88288, 67248, 8801, 30884, 30203, 
	8688, 85095, 18908, 106600, 90656, 61144, 36166, 137897, 72744, 42217, 
	72626, 146860, 2951, 173219, 6303, 116133, 1916, 80782, 15029, 67245, 
	31028, 104551, 14783, 83962, 73425, 45736, 38192, 5284, 12346, 18773, 
	74892, 38445, 75585, 5918, 18999, 41399, 64447, 139444, 2527, 93440, 
	53288, 31806, 34110, 40471, 40983, 35527, 1230, 115613, 94375, 25932, 
	47094, 31209, 13013, 51423, 76326, 82904, 29303, 11973, 60207, 21922, 
	10099, 39484, 25276, 90711, 27471, 42073, 13007, 44022, 80585, 80630, 
	22096, 3515, 10527, 101717, 132546, 44391, 46593, 841, 262331, 5922, 
	2404, 25644, 195305, 20699, 144171, 42511, 55261, 61342, 2457, 58694, 
	282954, 62834, 30707, 18560, 33639, 63196, 275047, 81401, 19986, 113428, 
	29405, 57197, 17269, 9409, 73793, 45070, 140346, 71080, 84621, 12136, 
	39231, 13978, 1513, 48179, 18624, 14175, 14533, 104261, 59562, 61698, 
	41790, 16402, 39768, 18407, 13544, 394, 210609, 168382, 32641, 14521, 
	16777, 180974, 46559, 40745, 11246, 3340, 5717, 53973, 61629, 97237, 
	22712, 691, 55, 66071, 10969, 57775, 3169, 8226, 35500, 19929, 
	30040, 99052, 33071, 57245, 25403, 6895, 28362, 44132, 122062, 99461, 
	9933, 49373, 32076, 27231, 88766, 62646, 1034, 107621, 15337, 217266, 
	13015, 69866, 30969, 4047, 102839, 19328, 31593, 69543, 73805, 87999, 
	66440, 52514, 32789, 29271, 24563, 3880, 72436, 196210, 52393, 112434, 
	194415, 67700, 191419, 44634, 92402, 4904, 31125, 6806, 25877, 152964, 
	63322, 45156, 5487, 212201, 57173, 13030, 4566, 246817, 23718, 120594, 
	28808, 57914, 67186, 8294, 142255, 26350, 94732, 1635, 2272, 113460, 
	126065, 32864, 69844, 137209, 12286, 45293, 46530, 3738, 9976, 11934, 
	21574, 674, 8540, 63574, 68149, 40583, 148497, 125165, 6685, 8401, 
	154662, 37101, 39016, 32108, 11193, 167425, 41041, 
};

const unsigned int expire_nonces[] = {
	6685, 83337, 74116, 50981, 51352, 158110, 132142, 161310, 95702, 32959, 
	5785, 229298, 59738, 71206, 24625, 6, 141161, 43901, 35697, 98865, 
	41764, 104919, 89611, 813, 54564, 13614, 24774, 39079, 67709, 94367, 
	24006, 137451, 87265, 4096, 17540, 93657, 64419, 178853, 45385, 18571, 
	49357, 67111, 92635, 73880, 7810, 15338, 56201, 1419, 92096, 121328, 
	60987, 32796, 63605, 2178, 25415, 19115, 62190, 76200, 155888, 208604, 
	43921, 623, 63809, 129207, 59080, 111270, 58799, 47014, 26935, 140745, 
	15982, 22417, 54877, 64708, 3508, 63688, 117163, 93037, 27595, 49051, 
	109801, 54794, 38790, 192113, 14920, 2056, 419624, 20495, 86719, 27455, 
	94870, 5539, 12871, 24142, 103201, 12358, 60226, 11163, 57429, 49086, 
	115051, 39940, 68936, 392, 23999, 23185, 37265, 793, 196124, 133047, 
	95771, 4927, 2410, 191047, 8416, 36182, 126426, 101492, 268543, 66462, 
	132688, 9709, 25766, 10781, 56169, 17484, 191643, 46857, 180718, 129600, 
	76319, 275342, 113429, 52200, 8584, 205931, 60264, 99367, 71513, 10931, 
	2470, 146435, 35660, 30357, 53621, 126053, 198310, 7340, 23329, 56232, 
	43152, 1290, 178803, 58294, 28730, 135307, 26024, 33903, 23202, 69984, 
	91861, 4406, 21564, 12088, 5307, 17517, 177507, 3629, 81666, 196118, 
	37329, 42816, 1766, 5227, 98516, 62284, 1449, 10331, 4915, 1086, 
	257130, 125081, 63069, 34059, 51482, 15396, 25319, 208595, 45717, 145038, 
	51317, 34896, 60597, 8930, 150489, 15827, 94634, 19809, 90784, 102628, 
	26279, 36205, 239377, 98432, 1949, 167692, 123222, 36572, 5435, 239413, 
	85529, 124924, 17443, 10391, 21356, 109441, 711, 27883, 15410, 172902, 
	9155, 6372, 93226, 31199, 47383, 77311, 107243, 1248, 3968, 88072, 
	50741, 175826, 9475, 19473, 78911, 59587, 172626, 54698, 127135, 4356, 
	70568, 9206, 41708, 162673, 82436, 8787, 12851, 17524, 27151, 34992, 
	19003, 17118, 1353, 173957, 62721, 10956, 28301, 38722, 35000, 51572, 
	122622, 26131, 219537, 24299, 8306, 22556, 117394, 5874, 1658, 4299, 
	85895, 59207, 17620, 65379, 53730, 66114, 31973, 80054, 39898, 88576, 
	35918, 54740, 43218, 310351, 18849, 65424, 18941, 49216, 21837, 1044, 
	36089, 89042, 1064, 57622, 18277, 30812, 392721, 68449, 21958, 59353, 
	230626, 192876, 152661, 83303, 12403, 48000, 322, 36098, 216060, 261073, 
	10127, 40078, 13820, 37595, 2465, 67578, 8074, 17069, 23001, 75590, 
	59540, 38500, 81671, 83017, 21630, 42072, 87988, 34685, 54463, 73723, 
	64583, 11708, 27819, 60914, 44671, 73532, 481251, 50437, 51482, 140164, 
	17802, 52420, 18605, 39313, 5815, 130397, 47241, 41700, 73784, 38254, 
	31816, 81033, 63873, 61180, 73597, 31012, 46596, 34360, 16076, 3553, 
	19667, 70678, 76463, 14007, 6695, 34346, 177277, 82740, 10695, 195656, 
	199473, 19061, 12235, 118857, 5890, 158834, 14991, 9908, 40669, 76476, 
	5798, 56010, 12434, 136848, 44171, 33686, 38022, 85052, 88529, 96055, 
	77808, 14052, 26881, 183273, 110552, 14780, 62505, 29327, 16832, 146503, 
	4492, 3210, 60633, 117771, 14125, 30949, 20800, 35101, 72610, 3023, 
	39933, 7829, 21639, 14090, 59951, 46100, 26005, 57832, 3410, 58340, 
	83407, 189530, 1991, 46036, 39758, 26344, 36726, 13556, 54091, 52219, 
	10445, 23350, 62863, 41887, 39607, 47051, 170358, 62714, 54450, 44956, 
	90394, 89040, 82532, 10732, 30853, 69521, 27096, 129159, 25700, 56643, 
	4510, 61375, 45066, 84264, 47513, 27524, 25215, 95656, 73959, 20581, 
	101988, 14797, 76360, 120161, 17567, 3903, 126413, 64154, 317038, 33995, 
	25108, 8165, 132499, 174571, 4312, 63941, 109366, 12461, 81720, 36019, 
	57768, 30058, 64038, 60147, 5536, 87586, 10954, 84112, 42404, 109189, 
	13869, 50548, 11654, 10051, 76725, 20594, 51481, 2668, 262831, 23754, 
	66190, 133133, 119290, 52240, 72002, 103103, 9144, 72562, 54350, 22676, 
	126558, 532, 23582, 20393, 25177, 4026, 55663, 24424, 175796, 14987, 
	58251, 124293, 4159, 220204, 2488, 36895, 10692, 1368, 80599, 203043, 
	3851, 11006, 24724, 40853, 19234, 36755, 43655, 12807, 40882, 43029, 
	4359, 35786, 24830, 102413, 17399, 48845, 22866, 8417, 24049, 50162, 
	36921, 162891, 38509, 121018, 54548, 158171, 54355, 12742, 6174, 
};
const unsigned int support_nonces[] = {};

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

class CNoncePrinter
{
public:
    CNoncePrinter() {};
    void addNonce(int nonce)
    {
        nonces.push_back(nonce);
    }
    void printNonces()
    {
        int counter = 0;
        std::cout << "{";
        for (std::vector<int>::iterator nonce_it = nonces.begin(); nonce_it != nonces.end(); ++nonce_it)
        {
            if (counter % 10 == 0)
            {
                std::cout << std::endl << '\t';
            }
            std::cout << *nonce_it << ", ";
            counter++;
        }
        std::cout << std::endl << "}" << std::endl;
    }
private:
    std::vector<int> nonces;
};

bool CreateBlock(CBlockTemplate* pblocktemplate, CNoncePrinter * pnp, int nonce)
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
    if (!pnp)
        pblock->nNonce = nonce;
    else
    {
        for (int i = 0; ; ++i)
        {
            pblock->nNonce = i;
            if (CheckProofOfWork(pblock->GetHash(), pblock->nBits, Params().GetConsensus()))
            {
                pnp->addNonce(i);
                break;
            }
        }
    }
    CValidationState state;
    bool success = (ProcessNewBlock(state, NULL, pblock, true, NULL) && state.IsValid() && pblock->GetHash() == chainActive.Tip()->GetBlockHash());
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
    //CNoncePrinter noncePrinter;
    CNoncePrinter* pnp = NULL;//&noncePrinter;
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

    for (unsigned int i = 0; i < 104; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
        if (coinbases.size() < 4)
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
    CMutableTransaction tx7 = BuildTransaction(coinbases[3]);
    tx7.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx7.vout[0].nValue = tx1.vout[0].nValue - 1;
    CNodeValue val;

    std::vector<uint256> blocks_to_invalidate;
    
    // Verify updates to the best claim get inserted immediately, and others don't.
    
    // Put tx1, tx2, and tx7 into the blockchain, and then advance 100 blocks to put them in the trie
    
    AddToMempool(tx1);
    AddToMempool(tx2);
    AddToMempool(tx7);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 4);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx7.GetHash()));

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    
    BOOST_CHECK(!pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pnccTrie->getInfoForName(sName2, val));
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    delete pblocktemplate;
    
    // Verify tx1 and tx2 are in the trie

    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    BOOST_CHECK(pnccTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Verify tx7 is also in the trie

    BOOST_CHECK(pnccTrie->haveClaim(sName1, tx7.GetHash(), 0));

    // Spend tx1 with tx3, tx2 with tx4, and put in tx5.
    
    AddToMempool(tx3);
    AddToMempool(tx4);
    AddToMempool(tx5);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 4);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    AddToMempool(tx4);
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 51; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // move forward some, but not far enough for the claim to get into the trie

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // update the original claim (tx3 spends tx1)

    AddToMempool(tx3);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // spend the update (tx6 spends tx3)

    AddToMempool(tx6);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, insert_nonces[block_counter++]));
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
    //noncePrinter.printNonces();
}

BOOST_AUTO_TEST_CASE(ncctrie_claim_expiration)
{
    int block_counter = 0;
    //CNoncePrinter noncePrinter;
    CNoncePrinter* pnp = NULL;//&noncePrinter;
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
        if (coinbases.size() < 2)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }

    delete pblocktemplate;

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(tx1);
    tx2.vout[0].scriptPubKey = CScript() << OP_TRUE;

    std::vector<uint256> blocks_to_invalidate;
    // set expiration time to 100 blocks after the block becomes valid. (more correctly, 200 blocks after the block is created)

    pnccTrie->setExpirationTime(200);

    // create a claim. verify no expiration event has been scheduled.

    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
        if (i == 50)
            blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, pnp, expire_nonces[block_counter++]));
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

    //noncePrinter.printNonces();
}

BOOST_AUTO_TEST_CASE(ncctrie_supporting_claims)
{
    int block_counter = 0;
    CNoncePrinter noncePrinter;
    CNoncePrinter* pnp = &noncePrinter;
    
    BOOST_CHECK(pnccTrie->nCurrentHeight == chainActive.Height() + 1);

    CBlockTemplate *pblocktemplate;
    LOCK(cs_main);

    CScript scriptPubKey = CScript() << OP_TRUE;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    std::vector<CTransaction> coinbases;
    for (unsigned int i = 0; i < 103; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, support_nonces[block_counter++]));
        if (coinbases.size() < 3)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }

    delete pblocktemplate;

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    std::string sTx1Hash = tx1.GetHash().ToString();
    std::vector<unsigned char> vchTx1Hash(sTx1Hash.begin(), sTx1Hash.end());
    tx1.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchTx1Hash << 0 << OP_2DROP << OP_DROP << OP_TRUE;

    std::vector<uint256> blocks_to_invalidate;

    // Test 1: create 1 LBC claim (A), create 5 LBC support (A), create 5 LBC claim (B)
    // Verify that A retains control throughout

    // Test 2: create 1 LBC claim (A), create 5 LBC claim (B), create 5 LBC support (A)
    // Verify that A loses control when B becomes valid, and then A gains control when support becomes valid

    // Test 3: create 1 LBC claim (A), create 5 LBC support (A), create 5 LBC claim(B), spend (A) support
    // Verify that A retains control until support is spent

    // Test 4: create 1 LBC claim (A), wait till valid, create 5 LBC claim (B), create 5 LBC support (A)
    // Verify that A retains control throughout

    // Test 5: create 5 LBC claim (A), wait till valid, create 1 LBC claim (B), create 5 LBC support (B)
    // Verify that A retains control until support becomes valid

    // Test 6: create 1 LBC claim (A), wait till valid, create 5 LBC claim (B), create 5 LBC support (A), spend A
    // Verify that A retains control until it is spent

    // Test 7: create 1 LBC claim (A), wait till valid, create 5 LBC support (A), spend A
    // Verify name trie is empty
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
