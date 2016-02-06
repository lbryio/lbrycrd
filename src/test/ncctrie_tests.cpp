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
    47864, 70562, 102076, 29568, 22490, 22769, 5000, 19580, 50900, 6462,
    64813, 22073, 48263, 96969, 222691, 146458, 76007, 105190, 11401, 14515,
    203019, 19029, 164801, 51632, 19463, 56817, 58258, 46103, 97679, 10580,
    31557, 176766, 57483, 37231, 61472, 29672, 3340, 14743, 47647, 28833,
    11843, 66537, 5105, 77780, 12270, 73825, 244523, 47452, 96794, 34497,
    67771, 16018, 37534, 32834, 26981, 27410, 20836, 29076, 134834, 180185,
    143043, 63176, 30082, 17094, 43620, 87972, 275074, 5306, 228970, 26128,
    124783, 3604, 9825, 24384, 26636, 70597, 514964, 5309, 60113, 33521,
    44825, 27066, 26288, 41406, 3766, 103595, 90818, 4539, 35609, 113968,
    147955, 108641, 80092, 77889, 49159, 12194, 225563, 1699, 18409, 77169,
    16383, 166243, 81121, 34986, 134632, 122648, 33768, 47477, 153994, 38973,
    202893, 111195, 124071, 102094, 18835, 56978, 170668, 36551, 75722, 141828,
    23880, 64969, 68576, 62520, 83244, 74063, 5778, 3964, 225044, 54563,
    17064, 38127, 23523, 91523, 16855, 9379, 18787, 70601, 9031, 51717,
    26563, 77126, 7447, 125050, 141, 7512, 19611, 6048, 47656, 9348,
    79912, 35843, 50490, 107387, 16606, 77454, 63480, 63073, 25449, 45363,
    115179, 52546, 18200, 19398, 12571, 131374, 27457, 78462, 42836, 14535,
    44255, 30167, 68307, 16068, 214097, 22964, 71976, 20886, 103506, 79337,
    12677, 4186, 126617, 50971, 304404, 67646, 274301, 11462, 127908, 227970,
    36763, 4374, 35899, 81014, 26616, 113321, 107200, 118140, 34971, 20484,
    19382, 28121, 29119, 4171, 263691, 90010, 5362, 1252, 16379, 67875,
    48816, 70008, 78077, 42141, 274402, 64404, 29048, 68155, 40330, 13389,
    27220, 54792, 92551, 59916, 106950, 12253, 91426, 70679, 188204, 18131,
    18140, 81867, 69231, 43748, 61209, 84967, 17533, 189318, 214936, 46093,
    52349, 18022, 11900, 114815, 80839, 60330, 20284, 70705, 44565, 41630,
    418242, 3748, 107687, 87922, 21830, 14465, 50678, 27455, 10509, 66027,
    66812, 19895, 60741, 42904, 16239, 20304, 167228, 21652, 21128, 15175,
    36498, 29416, 16439, 30056, 52031, 267457, 36733, 35286, 327852, 36891,
    65112, 213754, 8059, 49092, 139001, 81888, 43571, 63300, 23746, 86535,
    40392, 33540, 70265, 165061, 24267, 38953, 119058, 172744, 8092, 89955,
    94304, 56409, 111350, 147215, 381284, 195888, 113626, 45183, 181599, 21960,
    123902, 6903, 64749, 5211, 18984, 92254, 36524, 163374, 211065, 27583,
    4006, 3734, 233995, 17833, 23251, 90766, 62439, 78222, 6753, 143287,
    29582, 283989, 85745, 11782, 225909, 12318, 4874, 30732, 38807, 47820,
    64215, 15218, 14474, 11933, 192744, 43514, 6537, 44224, 8152, 52797,
    36002, 70940, 10113, 28230, 88513, 20851, 9416, 26690, 5636, 49017,
    24489, 5891, 52178, 112005, 1323, 65666, 27639, 19368, 3426, 939,
    3201, 82763, 76412, 93407, 64863, 110937, 113871, 61948, 84322, 120320,
    32750, 58558, 56873, 16736, 100739, 23973, 74486, 82648, 6239, 55025,
    75957, 99033, 18698, 15017, 29811, 156593, 34076, 275238, 73977, 58125,
    110002, 161309, 25162, 21797, 9184, 71998, 26884, 13724, 127125, 9812,
    57019, 15554, 96831, 157322, 213015, 133877, 36031, 48019, 41288, 21987,
    57397, 2743, 13895, 10856, 82534, 115190, 4674, 47244, 19616, 34988,
    45898, 77946, 18003, 56538, 66241, 110794, 206987, 17054, 65244, 200061,
    18330, 47037, 66014, 109698, 14009, 19877, 3833, 269982, 155678, 135408,
    2324, 14029, 72797, 1080, 18427, 122959, 66905, 174627, 79735, 76385,
    71183, 124006, 7897, 29318, 11133, 36011, 56162, 17163, 17898, 155128,
    106245, 241379, 118949, 107329, 58675, 132912, 14625, 31558, 11842, 96304,
    39669, 22840, 299, 56646, 37035, 178643, 28723, 33076, 53497, 18187,
    14902, 11989, 48905, 37425, 14750, 25955, 45064, 140598, 62745, 12696,
    101134, 31934, 19402, 55777, 16389, 101083, 153404, 32865, 140224, 36455,
    105193, 17279, 59216, 27586, 6950, 6954, 78686, 59285, 154434, 183079,
    99757, 121250, 41132, 9781, 45133, 20393, 168398, 95580, 3792, 138720,
    34715, 52729, 72670, 32519, 76086, 45306, 98322, 69894, 89144, 162112,
    32373, 32699, 91015, 83786, 6276, 44093, 130595, 43470, 28695, 106842,
    117764, 176045, 182298, 32614, 22494, 30691, 31846, 13440, 43454, 45975,
    79025, 53569, 35236, 83251, 47205, 17089, 78860};

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
    115051, 39940, 34817, 1724, 91130, 20041, 38570, 923, 17184, 42591,
    35586, 19752, 26576, 5173, 8970, 215002, 32461, 195216, 60150, 27044,
    5916, 228877, 25914, 56110, 127394, 111381, 36319, 111266, 21833, 52590,
    39068, 51757, 171729, 7358, 29374, 37896, 125432, 89249, 30156, 9523,
    76100, 148120, 24943, 39244, 117588, 42535, 32965, 145241, 162982, 84811,
    35939, 123068, 20170, 45770, 260368, 26161, 72054, 46892, 135347, 131614,
    72678, 15791, 57306, 3384, 30735, 70373, 59112, 149034, 7153, 43872,
    45341, 35383, 69, 609, 12034, 8438, 21840, 10436, 80502, 31340,
    109070, 7458, 12991, 233778, 135362, 29701, 77055, 86303, 17555, 25517,
    5205, 81740, 23024, 7139, 22769, 122158, 77201, 16994, 3560, 30110,
    27323, 1349, 20958, 116331, 31375, 133579, 20904, 67165, 8954, 3102,
    84605, 47576, 81461, 21605, 59055, 65933, 11157, 92120, 33227, 50342,
    90112, 66074, 56248, 2922, 90955, 108013, 13643, 14320, 50956, 44099,
    65428, 122313, 3797, 27404, 41130, 2296, 45138, 205861, 78637, 46951,
    60730, 29619, 144080, 2208, 49247, 30945, 44877, 18714, 9715, 127097,
    34095, 17017, 333, 16886, 48304, 44245, 3722, 17912, 167555, 10276,
    7961, 40281, 64116, 73456, 45919, 70395, 99043, 16845, 1932, 26892,
    37878, 53549, 89852, 23005, 78973, 74107, 2392, 49788, 98929, 2727,
    157505, 19202, 39268, 54388, 52105, 77206, 88256, 7704, 34719, 60101,
    46413, 106590, 113925, 358628, 4899, 159559, 399, 21855, 25910, 51499,
    31095, 56302, 183160, 84071, 92756, 152426, 6388, 85811, 94305, 2721,
    107352, 51510, 16615, 152584, 50273, 35666, 38855, 56913, 27707, 57761,
    28430, 18212, 12477, 140303, 25670, 43210, 27620, 148028, 51561, 77556,
    23667, 394, 58426, 18566, 31627, 54029, 6953, 38339, 72961, 154717,
    142676, 9515, 66512, 44269, 35671, 78909, 17336, 26498, 40930, 16674,
    11195, 38641, 120251, 110780, 9775, 103928, 44075, 46645, 55815, 7801,
    26240, 23347, 10875, 20669, 11529, 57307, 3963, 88243, 24511, 55463,
    44979, 66228, 59614, 118508, 95007, 3359, 33983, 58249, 1013, 56155,
    42072, 125072, 56416, 132031, 17483, 50512, 19961, 28508, 41761, 162244,
    27153, 27361, 158584, 23059, 27499, 150286, 26279, 38744, 40316, 32869,
    78376, 43217, 12101, 13071, 8363, 101726, 91323, 81057, 85645, 17909,
    82419, 181847, 92612, 27698, 1579, 55772, 5290, 64689, 52203, 29000,
    5085, 28834, 212585, 32008, 152752, 22753, 802, 31195, 28394, 73452,
    59259, 7126, 36382, 117150, 1214, 2814, 27005, 5996, 5399, 22524,
    32785, 15673, 5669, 23434, 137538, 63064, 17607, 2787, 85835, 14197,
    54059, 144162, 50208, 198656, 73481, 12705, 1358, 242235, 26206, 61382,
    48368, 25012, 26385, 138366, 103553, 27639, 190425, 194892, 8592, 31087,
    254588, 43661, 115645, 95185, 54081, 48525, 232239, 119953, 2679, 33312,
    117797, 21667, 26637, 90650, 24069, 26953, 47329, 16529, 35478, 9702,
    209987, 48940, 32153, 32670, 283105, 28191, 38158, 53877, 38322, 92536,
    38999, 121915, 89484, 25975, 119789, 83084, 32398, 55645, 272764, 17181,
    141410, 31545, 35612, 23513, 211548, 36853, 154871, 76235, 59678, 131872,
    11252, 21975, 35712, 7510, 1534, 44727, 143860, 47305, 27722, 175266,
    43248, 225233, 11955, 88368, 168603, 124807, 69459, 31880, 49456, 355451,
    79880, 128749, 65667, 10897, 14131, 4409, 10523, 8720, 112240, 71025,
    95521, 103635, 39284, 63443, 66790, 52473, 80317, 118192, 76604, 42631,
    124855, 28023, 52448, 140048, 15594, 192785, 5973, 40950, 37768, 94673,
    82233, 7635, 172419, 88992, 5259, 59021, 94380, 38724, 287534, 8261,
    3699, 52757, 83678, 134701, 109758, 11209, 52740, 44356, 56768, 22635,
    28090, 28563, 92925, 32423, 11457, 3837, 13992, 41831, 35708, 3784,
    29034, 29481, 29843, 48043, 69674, 20555, 38391, 26421, 10631, 92622,
    20304, 27564, 10952, 128072, 19793, 21355, 12599, 17296, 51064, 139138,
    22038, 6035, 3012, 43928, 110504, 1708, 10164, 79900, 21204, 78917,
    8746, 61911, 45844, 1885, 13413, 11197, 58423, 24607, 142379, 28543,
    57602, 298725, 6766, 82398, 31727, 87268, 13744, 35847, 123645, 106809,
    10275, 23769, 91704, 20887, 74599, 95506, 188778, 56997, 36100, 72149,
    69856, 117184, 11810, 65634, 18767, 30398, 30976, 62762, 54996, 5967,
    102018, 7538, 117803, 13328, 98490, 18573, 254, 110552, 4154, 152476,
    102240, 3962, 33563, 95590, 182191, 12839, 13578, 15032, 1445, 4095,
    92483, 21161, 1623, 22039, 67635, 61451, 62277, 154696, 195002, 15911,
    3103, 5683, 15840, 4474, 68755, 4592, 8261, 81237, 120883, 41460,
    11793, 125524, 280727, 326875, 12209, 24251, 101730, 4289, 91666, 1238,
    24388, 34929, 43158, 88433, 39474, 19966, 12112, 150581, 46709, 5426,
    78896, 28203, 60363, 139941, 62739, 133675, 148176, 125234, 221557, 15251,
    2323, 21270, 15099, 18092, 11211, 170844, 2169, 33698, 66588, 62719,
    120057, 8823, 5430, 44579, 61385, 84720, 64857, 70478, 116285, 8904,
    63423}; 

const unsigned int proof_nonces[] = {
    54804, 229797, 119107, 77644, 9782, 104842, 72332, 47116, 363729, 170598,
    434, 48775, 15164, 51638, 63813, 21028, 31896, 117216, 26019, 7458,
    101082, 48571, 84174, 56983, 4607, 176131, 60681, 11348, 42867, 152045,
    162526, 48222, 9115, 162702, 7084, 49303, 34393, 67035, 72372, 81712,
    58846, 123668, 37484, 69474, 77333, 104992, 71743, 9675, 12363, 39457,
    29256, 190978, 55083, 171352, 82498, 94184, 110307, 25965, 53432, 193885,
    3633, 74876, 15957, 252915, 119231, 32149, 84494, 139388, 141968, 9092,
    216682, 4336, 62779, 24742, 46920, 249663, 223300, 64403, 128849, 56433,
    30665, 174236, 77179, 18440, 217216, 45037, 35402, 135703, 15338, 126459,
    14097, 21840, 1175, 37591, 82927, 37893, 121151, 54253, 15384, 2192,
    12708, 74849, 39787, 15685, 37774, 13139, 34776, 159887, 6814, 3021,
    53901, 24286, 110415, 72451, 130820, 111543, 1300, 98281, 48336, 220704,
    51457, 91510, 148670, 41904, 16121, 41743, 63992, 27520, 15825, 108299,
    107508, 49504, 31499, 132824, 96172, 71483, 26828, 2545, 29227, 7846,
    114426, 221520, 9779, 594, 284222, 40381, 19645, 38653, 41721, 55469,
    16191, 7360, 42980, 36694, 13660, 72041, 1305, 83146, 92665, 15203,
    25428, 45068, 281892, 168851, 27104, 42352, 4321, 65234, 32459, 4261,
    101678, 29995, 34940, 34278, 169292, 24555, 20685, 44997, 42753, 21555,
    5974, 30, 59247, 17323, 10938, 78807, 17244, 68852, 17954, 26818,
    131893, 87813, 31242, 86553, 119319, 20972, 142663, 73972, 35525, 4782,
    20213, 13208, 35272, 229212, 203438,
};

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

bool CreateBlock(CBlockTemplate* pblocktemplate, bool find_nonce, int nonce)
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
    if (!find_nonce)
        pblock->nNonce = nonce;
    else
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
    bool find_nonces = false;
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx7.GetHash()));

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    
    BOOST_CHECK(!pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pnccTrie->getInfoForName(sName2, val));
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    AddToMempool(tx4);
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 51; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // move forward some, but not far enough for the claim to get into the trie

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // update the original claim (tx3 spends tx1)

    AddToMempool(tx3);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // spend the update (tx6 spends tx3)

    AddToMempool(tx6);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, insert_nonces[block_counter++]));
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
    bool find_nonces = false;
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
        if (coinbases.size() < 2)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }

    delete pblocktemplate;

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(tx1);
    tx2.vout[0].scriptPubKey = CScript() << OP_TRUE;
    CMutableTransaction tx3 = BuildTransaction(coinbases[1]);
    tx3.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP << OP_TRUE;
    tx3.vout[0].nValue = tx1.vout[0].nValue >> 1;

    std::vector<uint256> blocks_to_invalidate;
    // set expiration time to 100 blocks after the block becomes valid.

    pnccTrie->setExpirationTime(200);

    // create a claim. verify the expiration event has been scheduled.

    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // advance until the claim is valid.

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
        if (i == 50)
            blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
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

    // Make sure that when a claim expires, a lesser claim for the same name takes over

    CNodeValue val;

    // create one claims for the name

    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // advance a little while and insert the second claim

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    AddToMempool(tx3);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());
    
    // advance until tx1 is valid

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    // advance until tx3 is valid, ensure tx1 is winning

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    delete pblocktemplate;
    
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // advance until the expiration event occurs. verify the expiration event occurs on time.

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
        if (i == 1)
            blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    }
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());

    // spend tx1

    AddToMempool(tx2);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, expire_nonces[block_counter++]));
    delete pblocktemplate;

    // roll back to when tx1 and tx3 are in the trie and tx1 is winning

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(!pnccTrie->expirationQueueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

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

bool verify_proof(const CNCCTrieProof proof, uint256 rootHash, const std::string& name)
{
    uint256 previousComputedHash;
    std::string computedReverseName;
    bool verifiedValue = false;

    for (std::vector<CNCCTrieProofNode>::const_reverse_iterator itNodes = proof.nodes.rbegin(); itNodes != proof.nodes.rend(); ++itNodes)
    {
        bool foundChildInChain = false;
        std::vector<unsigned char> vchToHash;
        for (std::vector<std::pair<unsigned char, uint256> >::const_iterator itChildren = itNodes->children.begin(); itChildren != itNodes->children.end(); ++itChildren)
        {
            vchToHash.push_back(itChildren->first);
            uint256 childHash;
            if (itChildren->second.IsNull())
            {
                if (previousComputedHash.IsNull())
                {
                    return false;
                }
                if (foundChildInChain)
                {
                    return false;
                }
                foundChildInChain = true;
                computedReverseName += itChildren->first;
                childHash = previousComputedHash;
            }
            else
            {
                childHash = itChildren->second;
            }
            vchToHash.insert(vchToHash.end(), childHash.begin(), childHash.end());
        }
        if (itNodes != proof.nodes.rbegin() && !foundChildInChain)
        {
            return false;
        }
        if (itNodes->hasValue)
        {
            uint256 valHash;
            if (itNodes->valHash.IsNull())
            {
                if (itNodes != proof.nodes.rbegin())
                {
                    return false;
                }
                if (!proof.hasValue)
                {
                    return false;
                }
                valHash = getOutPointHash(proof.txhash, proof.nOut);
                verifiedValue = true;
            }
            else
            {
                valHash = itNodes->valHash;
            }
            vchToHash.insert(vchToHash.end(), valHash.begin(), valHash.end());
        }
        else if (proof.hasValue && itNodes == proof.nodes.rbegin())
        {
            return false;
        }
        CHash256 hasher;
        std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
        hasher.Write(vchToHash.data(), vchToHash.size());
        hasher.Finalize(&(vchHash[0]));
        uint256 calculatedHash(vchHash);
        previousComputedHash = calculatedHash;
    }
    if (previousComputedHash != rootHash)
    {
        return false;
    }
    if (proof.hasValue && !verifiedValue)
    {
        return false;
    }
    std::string::reverse_iterator itComputedName = computedReverseName.rbegin();
    std::string::const_iterator itName = name.begin();
    for (; itName != name.end() && itComputedName != computedReverseName.rend(); ++itName, ++itComputedName)
    {
        if (*itName != *itComputedName)
        {
            return false;
        }
    }
    return (!proof.hasValue || itName == name.end());
}

BOOST_AUTO_TEST_CASE(ncctrievalue_proof)
{
    int block_counter = 0;
    bool find_nonces = false;
    BOOST_CHECK(pnccTrie->nCurrentHeight == chainActive.Height() + 1);

    CBlockTemplate *pblocktemplate;
    LOCK(cs_main);

    CScript scriptPubKey = CScript() << OP_TRUE;

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

    std::vector<unsigned char> vchName1(sName1.begin(), sName1.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());

    std::vector<unsigned char> vchName2(sName2.begin(), sName2.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<unsigned char> vchName3(sName3.begin(), sName3.end());
    std::vector<unsigned char> vchValue3(sValue3.begin(), sValue3.end());

    std::vector<unsigned char> vchName4(sName4.begin(), sName4.end());
    std::vector<unsigned char> vchValue4(sValue4.begin(), sValue4.end());

    CNodeValue val;

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    std::vector<CTransaction> coinbases;
    for (unsigned int i = 0; i < 104; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, proof_nonces[block_counter++]));
        if (coinbases.size() < 4)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }

    delete pblocktemplate;

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    tx3.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName3 << vchValue3 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx4 = BuildTransaction(coinbases[3]);
    tx4.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName4 << vchValue4 << OP_2DROP << OP_DROP << OP_TRUE;

    std::vector<uint256> blocks_to_invalidate;

    // create a claim. verify the expiration event has been scheduled.

    AddToMempool(tx1);
    AddToMempool(tx2);
    AddToMempool(tx3);
    AddToMempool(tx4);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 5);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, proof_nonces[block_counter++]));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, proof_nonces[block_counter++]));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, find_nonces, proof_nonces[block_counter++]));
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    BOOST_CHECK(pnccTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());
    BOOST_CHECK(pnccTrie->getInfoForName(sName3, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    BOOST_CHECK(pnccTrie->getInfoForName(sName4, val));
    BOOST_CHECK(val.txhash == tx4.GetHash());

    CNCCTrieCache cache(pnccTrie);

    CNCCTrieProof proof;

    proof = cache.getProofForName(sName1);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashNCCTrie, sName1));
    BOOST_CHECK(proof.txhash == tx1.GetHash());
    BOOST_CHECK(proof.nOut == 0);

    proof = cache.getProofForName(sName2);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashNCCTrie, sName2));
    BOOST_CHECK(proof.txhash == tx2.GetHash());
    BOOST_CHECK(proof.nOut == 0);

    proof = cache.getProofForName(sName3);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashNCCTrie, sName3));
    BOOST_CHECK(proof.txhash == tx3.GetHash());
    BOOST_CHECK(proof.nOut == 0);

    proof = cache.getProofForName(sName4);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashNCCTrie, sName4));
    BOOST_CHECK(proof.txhash == tx4.GetHash());
    BOOST_CHECK(proof.nOut == 0);

    proof = cache.getProofForName(sName5);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashNCCTrie, sName5));
    BOOST_CHECK(proof.hasValue == false);

    proof = cache.getProofForName(sName6);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashNCCTrie, sName6));
    BOOST_CHECK(proof.hasValue == false);

    proof = cache.getProofForName(sName7);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashNCCTrie, sName7));
    BOOST_CHECK(proof.hasValue == false);
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(blocks_to_invalidate.empty());
    
}

BOOST_AUTO_TEST_SUITE_END()
