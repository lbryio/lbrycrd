// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"
#include "miner.h"
#include "txmempool.h"
#include "claimtrie.h"
#include "nameclaim.h"
#include "coins.h"
#include "streams.h"
#include "chainparams.h"
#include <boost/test/unit_test.hpp>
#include <iostream>
#include "test/test_bitcoin.h"

using namespace std;

CScript scriptPubKey = CScript() << OP_TRUE;

const unsigned int insert_nonces[] = {
	155628, 39910, 106604, 73592, 56826, 136756, 169244, 65570, 121703, 50365, 
	47566, 8341, 87403, 4885, 7499, 23858, 84894, 6343, 53404, 73474, 
	7981, 3801, 8293, 75679, 50803, 18724, 22750, 9501, 100343, 116694, 
	147940, 100560, 53917, 47565, 198059, 7745, 4287, 3061, 15517, 142493, 
	19861, 87677, 316724, 45217, 2293, 74794, 201797, 25724, 17530, 59158, 
	7358, 8950, 50471, 80568, 46991, 37572, 109787, 154450, 29521, 30740, 
	74421, 37943, 174336, 63708, 120176, 51829, 181907, 46865, 28557, 92469, 
	45992, 28559, 10112, 20048, 24572, 14288, 84634, 131257, 111431, 278957, 
	137715, 30203, 15110, 89827, 34064, 73864, 86459, 271510, 49225, 7031, 
	166341, 228178, 112403, 110441, 33255, 46489, 180159, 115220, 148176, 38211, 
	29321, 56734, 60510, 85339, 42224, 57060, 84419, 107869, 239088, 5008, 
	106925, 22907, 145355, 62131, 40031, 8895, 43388, 56524, 40267, 1436, 
	158156, 175503, 11981, 59408, 86034, 95875, 39047, 138584, 260506, 166962, 
	82236, 171086, 104457, 12813, 13190, 11599, 29210, 19090, 90114, 25909, 
	173926, 112336, 41450, 22040, 81443, 122350, 29060, 5852, 86271, 50407, 
	246135, 69572, 15253, 71269, 82487, 21428, 14461, 250107, 53041, 17675, 
	1011, 26166, 14408, 19045, 53044, 30151, 125263, 229230, 31201, 122585, 
	37139, 71645, 47182, 41257, 53809, 16935, 33238, 27617, 112009, 139103, 
	36123, 22674, 45647, 34445, 132598, 177969, 35623, 46029, 14768, 6247, 
	28649, 123582, 27044, 128625, 28705, 6060, 51578, 51109, 21778, 8319, 
	3730, 81045, 13998, 9544, 120347, 22020, 29347, 27976, 5600, 10136, 
	1287, 8977, 56238, 47661, 79276, 19913, 145875, 232196, 5481, 44000, 
	114843, 73267, 69339, 40976, 45828, 20190, 42290, 17866, 17247, 75381, 
	99422, 22061, 24503, 32540, 59735, 12123, 32011, 30995, 8385, 57895, 
	14589, 149735, 87639, 36483, 84561, 120576, 8283, 39601, 13194, 64956, 
	34522, 36648, 44189, 2989, 11155, 22891, 29259, 95187, 229848, 33360, 
	258769, 117889, 279100, 66763, 6343, 9712, 23942, 49704, 30169, 8089, 
	17775, 2762, 172995, 96724, 17639, 4000, 3530, 80612, 11895, 31376, 
	36561, 16686, 25661, 23295, 14710, 23141, 38004, 10971, 130895, 10516, 
	233133, 106077, 11471, 48069, 21759, 39622, 84834, 5749, 61441, 13117, 
	192583, 232320, 4130, 24245, 11509, 187805, 44072, 78606, 8422, 89015, 
	1565, 52957, 2897, 7905, 25244, 104568, 31452, 34, 38198, 1828, 
	23704, 12388, 20386, 1604, 3651, 9238, 163574, 5187, 207492, 35848, 
	21468, 12953, 5981, 66143, 105063, 52380, 53867, 47645, 92922, 128609, 
	11717, 38047, 186705, 30412, 34637, 56797, 69810, 24538, 119653, 105672, 
	17211, 18454, 58143, 2715, 4688, 1698, 84285, 69246, 12861, 85065, 
	181565, 2510, 35027, 234, 106934, 43419, 54492, 72089, 182699, 19743, 
	34980, 42495, 113416, 211393, 12606, 44988, 93710, 2876, 22550, 152811, 
	5125, 7158, 127842, 298833, 2670, 140245, 210675, 20872, 142251, 71425, 
	96659, 17887, 5082, 2800, 3509, 174381, 21604, 73958, 56196, 47591, 
	35776, 255243, 18728, 44421, 29303, 138275, 34105, 61382, 10611, 7586, 
	131626, 21686, 53482, 130203, 105082, 50745, 28756, 131461, 58455, 109440, 
	31388, 41615, 119446, 56954, 3655, 188918, 90664, 38566, 56183, 17193, 
	17745, 28499, 33856, 19059, 5304, 10160, 29761, 27596, 193129, 213470, 
	19958, 4091, 18845, 82775, 16441, 38658, 116790, 16883, 1143, 50916, 
	81243, 119477, 45790, 90353, 185041, 1142, 96268, 40823, 62168, 98313, 
	2024, 45306, 91978, 53545, 37740, 23032, 70197, 64118, 38759, 158668, 
	8190, 116048, 29418, 24210, 28832, 12056, 132, 3482, 36506, 45750, 
	50130, 73613, 41695, 124902, 31360, 112543, 35580, 50150, 164449, 8352, 
	53090, 85848, 59162, 23371, 11110, 37529, 13346, 8135, 119414, 54797, 
	106935, 43638, 64324, 13900, 45857, 8340, 127701, 161, 254585, 62561, 
	203865, 12614, 87727, 59047, 95555, 140724, 32987, 41114, 89070, 116326, 
	12410, 7955, 31305, 304152, 34579, 125789, 69845, 55911, 141878, 275139, 
	48268, 18010, 20297, 68408, 41114, 9743, 37287, 52599, 5675, 26326, 
	32383, 128519, 34931, 81744, 53577, 69236, 3296, 48392, 203454, 353270, 
	1748, 174489, 154338, 85466, 30186, 16535, 59168, 18810, 21989, 12224, 
	40152, 174098, 36995, 66223, 109443, 157442, 67261, 14838, 48993, 25040, 
	79720, 84327, 14035, 44574, 41645, 124910, 22908, 18183, 54504, 182765, 
	23742, 33880, 22703, 8031, 55620, 32635, 41290, 19474, 147217, 4570, 
	80189, 57588, 7616, 44349, 123799, 69788, 39005, 6400, 3572, 52337, 
	25640, 42665, 136764, 120479, 61732, 46758, 71057, 61624, 93760, 11492, 
	92897, 1256, 38405, 97964, 2231, 121314, 20513, 24942, 297207, 64968, 
	113001, 287375, 12885, 47233, 25274, 52813, 25374, 13974, 67921, 12246, 
	27505, 32199, 23722, 56136, 189997, 151112, 3724, 43260, 44645, 63521, 
	98899, 119701, 4026, 5510, 4429, 94472, 48507, 132321, 108938, 35559, 
	103745, 21434, 27144, 143420, 26, 83687, 202909, 180852, 229502, 20467, 
	21809, 44979, 49762, 3778, 38885, 89006, 53420, 99161, 132586, 89432, 
	73607, 7511, 155005, 846, 94850, 43265, 35800, 31158, 162444, 14723, 
	81095, 39759, 92670, 4657, 24287, 5991, 28214, 94651, 29775, 90674, 
	66136, 171240, 21895, 173292, 16939, 202489, 218017, 73248, 20347, 24093, 
	189969, 104325, 9922, 49510, 5836, 4269, 138765, 51384, 37566, 7592, 
	14474, 119452, 142646, 128299, 73414, 87701, 13409, 5878, 44664, 10650, 
	5809, 60334, 23417, 43628, 26551, 3791, 61182, 72637, 1555, 26601, 
	71836, 37715, 18203, 68621, 153692, 225949, 66761, 35459, 1564, 171975, 
	360081, 8276, 1303, 82884, 185924, 64289, 35035, 9496, 24511, 59819, 
	101744, 38020, 54744, 51804, 1300, 30349, 133214, 42305, 46577, 8012, 
	46790, 216388, 74130, 83207, 129618, 217256, 16239, 3316, 
};

const unsigned int expire_nonces[] = {
	2564, 13385, 81600, 23899, 9954, 3054, 60346, 662, 16045, 61363, 
	19580, 99967, 36210, 45456, 107180, 51079, 25340, 96039, 17774, 20660, 
	20023, 21706, 14344, 49624, 766, 71855, 7116, 64335, 119626, 5020, 
	6373, 13861, 90563, 15432, 36049, 16241, 9818, 53998, 311447, 37240, 
	4264, 34932, 142440, 18535, 24222, 43775, 10437, 87953, 23866, 9961, 
	76330, 109343, 87624, 7513, 74088, 21330, 67554, 20723, 99673, 44398, 
	4099, 16139, 2313, 9043, 54291, 93056, 4878, 188177, 50352, 50953, 
	2572, 2087, 34771, 69726, 84958, 175062, 74084, 292971, 43131, 102500, 
	103800, 38617, 16526, 108869, 44337, 14756, 72664, 40835, 37837, 11052, 
	165928, 62862, 88629, 293382, 14562, 69048, 8023, 71971, 16485, 92234, 
	46881, 79768, 30199, 38001, 25730, 87500, 18552, 30302, 340024, 36658, 
	13251, 55514, 48282, 16902, 27668, 16910, 17340, 84040, 99271, 10660, 
	5096, 48967, 67140, 10150, 61081, 58481, 8810, 31547, 14312, 24710, 
	178565, 103778, 22937, 37785, 58133, 9488, 2233, 55984, 8158, 169474, 
	24324, 58792, 925, 177062, 78121, 76064, 98772, 119054, 5941, 6750, 
	1049, 300184, 24579, 67765, 103210, 42894, 40687, 3927, 49197, 76255, 
	18304, 21440, 41538, 8645, 10638, 29197, 54905, 18116, 41732, 6714, 
	29500, 45558, 33851, 70910, 82984, 20968, 158603, 165040, 143126, 30609, 
	78192, 90400, 141716, 899, 16620, 31287, 80, 53190, 147371, 55313, 
	98186, 77433, 84829, 30329, 19457, 118700, 65132, 625, 24729, 4798, 
	73348, 1784, 53980, 72906, 65482, 8776, 26046, 2788, 105441, 76393, 
	92791, 6932, 156322, 56986, 84120, 444, 47017, 9148, 32463, 32177, 
	58355, 17886, 105486, 251741, 30331, 20398, 152671, 1515, 121956, 40534, 
	62208, 53832, 25027, 10205, 47954, 73675, 1818, 110316, 31935, 10104, 
	55534, 61790, 106282, 17025, 87973, 140432, 17441, 3152, 132315, 23315, 
	20383, 2444, 86496, 11363, 107467, 11987, 14515, 38108, 66194, 131606, 
	173908, 46722, 133116, 71937, 159575, 10424, 1554, 94821, 4532, 263033, 
	105651, 43511, 20353, 9007, 120414, 96394, 52795, 120158, 84121, 98610, 
	7085, 93556, 19176, 73629, 19147, 3448, 11546, 44697, 54770, 2138, 
	50286, 103112, 111726, 21435, 108459, 1392, 180756, 71890, 22035, 2432, 
	11239, 54513, 5986, 176181, 18208, 48810, 34514, 11583, 4079, 109626, 
	127580, 90625, 93926, 6819, 53215, 114912, 21542, 90165, 126673, 36965, 
	120551, 74474, 45368, 22058, 1739, 32628, 300494, 36571, 15079, 33698, 
	9617, 50449, 75945, 21178, 38182, 48944, 93583, 76392, 9456, 47243, 
	25810, 80581, 37407, 121159, 47369, 75639, 33099, 87033, 10415, 35690, 
	12329, 92943, 15487, 158179, 5888, 41612, 51100, 79976, 75740, 27069, 
	38684, 19024, 10156, 345731, 26250, 51426, 24929, 53092, 13609, 9279, 
	20701, 17001, 84548, 91289, 11972, 22441, 62807, 86864, 4706, 18188, 
	104485, 100614, 69496, 25772, 90612, 28923, 113773, 34854, 4171, 116682, 
	130361, 249282, 27465, 55583, 29810, 96671, 19114, 15177, 88275, 45468, 
	82930, 48170, 3924, 69117, 90195, 7514, 22488, 30747, 17276, 87208, 
	34465, 89056, 159008, 969, 186787, 43531, 34928, 148648, 13913, 85251, 
	619, 69315, 224468, 61892, 1051, 142335, 15469, 26051, 138781, 42196, 
	82663, 117571, 12062, 53859, 83825, 88393, 123899, 93273, 50224, 150841, 
	27772, 39077, 37533, 137339, 119466, 17668, 47452, 2886, 91301, 59710, 
	97717, 12169, 25697, 33739, 68613, 3752, 106496, 9707, 17635, 22590, 
	129649, 14698, 107881, 107562, 27914, 38739, 1276, 115459, 9584, 112295, 
	186892, 45758, 28145, 95126, 18883, 1431, 35582, 30530, 218793, 14831, 
	106432, 106457, 70629, 137267, 156228, 32405, 29419, 52594, 37612, 52950, 
	90081, 15596, 98943, 40284, 24918, 9938, 207618, 36022, 34346, 248522, 
	235110, 9442, 734, 355481, 82720, 129744, 66608, 20553, 56016, 194907, 
	77911, 13421, 29657, 93608, 273, 8308, 179278, 62722, 18845, 43080, 
	225734, 24739, 9321, 344082, 101159, 56916, 68283, 21433, 163610, 10941, 
	31830, 133002, 136379, 184244, 2234, 118919, 48524, 105355, 38533, 90929, 
	31043, 93480, 22608, 69495, 82709, 37967, 70265, 19237, 64245, 25844, 
	18723, 11443, 3378, 83112, 92474, 118818, 53231, 57804, 186355,
};

const unsigned int support_nonces1[] = {
	39835, 60468, 65890, 26405, 58568, 44787, 10392, 113274, 19569, 71577, 
	12085, 205774, 4559, 8782, 26196, 62256, 26071, 62174, 44714, 77671, 
	29691, 206707, 69178, 82483, 21579, 300730, 6619, 11232, 4791, 49366, 
	73688, 168057, 68666, 52037, 150726, 103062, 2609, 115568, 16721, 164211, 
	32864, 105357, 48008, 69528, 30688, 2811, 253656, 99130, 58999, 29880, 
	57158, 222230, 34981, 5607, 81232, 91845, 113558, 57190, 124566, 22648, 
	19120, 9092, 300548, 23436, 35, 797, 92203, 39311, 24606, 11158, 
	54140, 49233, 56257, 5230, 125920, 20477, 30751, 17721, 32154, 102145, 
	96903, 4744, 50637, 8248, 17475, 51563, 108749, 33246, 112000, 68837, 
	343971, 89425, 10745, 15654, 8192, 291103, 105727, 105198, 117107, 132589, 
	16114, 84852, 41096, 28314, 71667, 49992, 26479, 70432, 89958, 28196, 
	16912, 475, 137722, 49276, 82607, 60290, 60386, 88897, 28232, 17288, 
	6647, 48635, 29520, 39950, 206708, 246066, 188602, 59507, 47616, 248094, 
	20368, 5089, 17860, 39920, 48756, 89570, 55877, 95875, 124268, 5730, 
	26529, 441792, 1279, 68653, 12819, 16875, 121644, 129490, 12557, 19990, 
	28213, 65710, 55331, 31021, 38430, 9562, 35248, 67954, 206349, 75344, 
	153603, 39491, 17749, 108455, 55196, 15230, 47410, 27547, 27817, 120506, 
	146185, 110652, 119988, 19716, 49318, 105939, 112419, 83773, 198460, 31037, 
	4441, 1274, 111370, 13264, 25544, 61275, 32757, 939, 77742, 50142, 
	130882, 1245, 38950, 65739, 132023, 40071, 58295, 200481, 298, 36261, 
	72063, 157533, 12480, 13907, 189869, 21281, 161007, 60508, 89552, 36312, 
	107153, 23059, 150578, 3054, 204955, 11172, 207131, 274238, 149167, 563276, 
	29719, 23172, 96823, 43700, 239002, 40345, 41303, 32329, 84210, 43465, 
	2985, 60271, 68445, 44983, 44226, 106916, 65913, 12391, 2474, 37752, 
	73209, 120487, 62162, 46595, 157796, 20803, 373, 23838, 5788, 10665, 
	79950, 36236, 78267, 38033, 61118, 64373, 45012, 14230, 724, 65975, 
	69790, 114404, 25587, 10833, 42495, 43750, 284208, 2486, 59226, 7819, 
	72457, 11922, 56156, 55745, 4844, 87477, 97402, 58147, 48177, 68289, 
	126652, 7989, 134337, 142620, 78308, 60354, 36198, 108997, 88169, 16222, 
	107794, 108743, 91716, 149076, 293058, 59769, 117934, 11776, 84524, 5644, 
	101743, 36687, 35922, 63910, 16476, 119768, 18970, 26228, 166714, 37445, 
	57435, 37310, 44988, 8068, 8495, 40094, 138610, 12464, 97767, 91000, 
	5881, 5816, 80082, 122207, 17905, 50427, 25702, 9719, 33494, 119270, 
	27585, 8257, 66469, 3857, 1936, 12223, 135639, 10109, 269868, 83152, 
	28698, 8299, 6513, 8871, 6397, 69652, 41330, 70946, 425761, 10053, 
	140508, 87541, 41190, 258318, 52161, 112392, 90585, 44157, 32931, 92045, 
	10545, 4046, 92835, 134967, 154400, 280611, 42452, 17596, 132265, 88621, 
	19474, 7100, 58597, 171403, 18609, 11847, 97688, 90354, 10842, 23273, 
	335523, 116016, 34301, 9065, 105784, 63288, 100918, 59759, 24926, 76893, 
	43263, 56612, 23327, 44719, 17687, 55305, 19827, 213422, 54628, 6844, 
	31737, 147644, 41800, 219903, 114, 160519, 33530, 77797, 29950, 134561, 
};

const unsigned int support_nonces2[] = {
	84485, 31978, 6485, 38305, 45178, 64995, 108015, 24887, 66832, 156207, 
	83638, 11333, 35112, 15947, 4018, 128067, 9754, 6005, 62146, 12706, 
	56469, 55700, 57671, 51340, 48138, 3704, 37223, 2494, 4480, 508, 
	40768, 51175, 19575, 205837, 85607, 78776, 19241, 195742, 40958, 24603, 
	3143, 58331, 105198, 92572, 92782, 10894, 6618, 9570, 21284, 16439, 
	251880, 25246, 3277, 17418, 26432, 66302, 151159, 250858, 21833, 9905, 
	128302, 27044, 37419, 16968, 30802, 14314, 37553, 10206, 92102, 113579, 
	22545, 714, 327429, 252719, 16126, 34301, 147813, 95988, 19529, 100005, 
	57982, 21483, 72193, 3666, 32397, 106879, 35758, 56006, 37686, 70568, 
	271200, 72687, 32142, 48953, 21081, 26391, 150757, 64189, 88986, 14145, 
	79637, 19822, 16232, 130352, 63970, 54587, 64734, 299019, 52714, 279567, 
	9167, 26293, 130577, 7432, 1262, 215601, 209606, 60978, 103679, 177627, 
	74791, 67214, 4350, 55026, 205, 161149, 22469, 40801, 47188, 9819, 
	17975, 17099, 48805, 159, 294792, 27373, 9233, 16249, 52948, 78183, 
	730, 81770, 182557, 125610, 97462, 52798, 305891, 108731, 35923, 15569, 
	118054, 71848, 40335, 47187, 84665, 1257, 7987, 76756, 48812, 124483, 
	73431, 49551, 7053, 148320, 121392, 18701, 23620, 23053, 91896, 2388, 
	29519, 5541, 109717, 93519, 103286, 20304, 22049, 185695, 20784, 117887, 
	122791, 4275, 32826, 121994, 22203, 22400, 75930, 25383, 29109, 10340, 
	45126, 32375, 272882, 61476, 38090, 13940, 77914, 40209, 89811, 235515, 
	33519, 67630, 44844, 157902, 19825, 3528, 135904, 32014, 71653, 116536, 
	2262, 53669, 7566, 52824, 50641, 167427, 115139, 12562, 51459, 11163, 
	82459, 10698, 47121, 3555, 37387, 185456, 178227, 155458, 66981, 7689, 
	10579, 30572, 27853, 4985, 6239, 113367, 26014, 9272, 46634, 65581, 
	44516, 77119, 26318, 19206, 16174, 16305, 42306, 76256, 49358, 303783, 
	9447, 94186, 38361, 154993, 58181, 19885, 30963, 7306, 2050, 5402, 
	66248, 126578, 122137, 23256, 28622, 27844, 15971, 5314, 74666, 1896, 
	97639, 52535, 14425, 6560, 54838, 78028, 20103, 71507, 5480, 55848, 
	73301, 17061, 1400, 88763, 93424, 26927, 28919, 47748, 13840, 17317, 
	35122, 2707, 119238, 15921, 62937, 16929, 18469, 77481, 13897, 101257, 
	18924, 131630, 160336, 33996, 84273, 83560, 70259, 53519, 20590, 3034, 
	39753, 18937, 3561, 58170, 62775, 1576, 107856, 54170, 65370, 12104, 
	40386, 63106, 6144, 44550, 125542, 25589, 1217, 107154, 20404, 127760, 
	8104, 14437, 87335, 199215, 44589, 15488, 3179, 30359, 181399, 152069, 
	14147, 87565, 35987, 177109, 5931, 15251, 31617, 5518, 34442, 59885, 
	1458, 41217, 14679, 128013, 94796, 85561, 13198, 31644, 11924, 4131, 
	120312, 89686, 35868, 4246, 12591, 94393, 40626, 14260, 64334, 47681, 
	97764, 48127, 53655, 54065, 19851, 56328, 123303, 24166, 15544, 5110, 
	23521, 27984, 4370, 173573, 1785, 90144, 74354, 105024, 389612, 34413, 
	19094, 160259, 169955, 11068, 32392, 54575, 61718, 8943, 29657, 2261, 
	57348, 33694, 64291, 127736, 64825, 177160, 111028, 159847, 36130, 145139, 
	16526, 31705, 11778, 55003, 4824, 21938, 67429, 7366, 38320, 41846, 
	47841, 25161, 80405, 157160, 1457, 94161, 64524, 50426, 5443, 36045, 
	41471, 85305, 121160, 15220, 92828, 16116, 141727, 10088, 24520, 63294, 
	18568, 135265, 35317, 13190, 50868, 24839, 80108, 150751, 10780, 5533, 
	2062, 320734, 76590, 39004, 1429, 2348, 90137, 174978, 140517, 74371, 
	76279, 101879, 59068, 324058, 104733, 43966, 166138, 26100, 36348, 47804, 
	45191, 10706, 227891, 18458, 32708, 112131, 58139, 65925, 34037, 1961, 
	74557, 143649, 109898, 88364, 26886, 74583, 101449, 23997, 130427, 14364, 
	59807, 23579, 40911, 49913, 106395, 51711, 41727, 29611, 204144, 30296, 
	24708, 28774, 5750, 46661, 61531, 42306, 208655, 5962, 32236, 16097, 
	31243, 88407, 20491, 117342, 129792, 8760, 209491, 104345, 17532, 41112, 
	6804, 22350, 6459, 3608, 2173, 35950, 6557, 84113, 150918, 10596, 
	28255, 22935, 48609, 67850, 72223, 150268, 29399, 2444, 23915, 1418, 
	18417, 7660, 56550, 17766, 15612, 74308, 68602, 55606, 163741, 92535, 
	75401, 8159, 178987, 127166, 77021, 21419, 15078, 33664, 146040, 39927, 
	56725, 5462, 133529, 4943, 134825, 27973, 40807, 40169, 90602, 131110, 
	93608, 23814, 34921, 134637, 44912, 1086, 14666, 112896, 56979, 120225, 
	57262, 183046, 25724, 69421, 86985, 94680, 57515, 2746, 7855, 10388, 
	44833, 10166, 51638, 61027, 58418, 64097, 32867, 39133, 71152, 188447, 
	4021, 27500, 48756, 10977, 15363, 82377, 24785, 55586, 29522, 88893, 
	23417, 190522, 130486, 60156, 9110, 9374, 26590, 197062, 116642, 256574, 
	1810, 26620, 6196, 88645, 13416, 42248, 25197, 25957, 164029, 29407, 
	3244, 43435, 32690, 77897, 22771, 139588, 38701, 38858, 13087, 223691, 
	497256, 9695, 2790, 34423, 61963, 44155, 17454, 17697, 99606, 5921, 
	53552, 61573, 30218, 35719, 34613, 30185, 29495, 22668, 17255, 73155, 
	167223, 17424, 4564, 29054, 292540, 47005, 15217, 32684, 46319, 47355, 
	215672, 64858, 35548, 76607, 179348, 7482, 28966, 167081, 21110, 142042, 
	6557, 67578, 1081, 22086, 34319, 37219, 19035, 81698, 5101, 40983, 
	173184, 53801, 31177, 133297, 57425, 48918, 64814, 170576, 76009, 35920, 
	53553, 6089, 185451, 29659, 111896, 88276, 97097, 214984, 12685, 15694, 
	280813, 9327, 25702, 102973, 2914, 7868, 76892, 21414, 2929, 4741, 
	79576, 54406, 311323, 4415, 107241, 37109, 130592, 105966, 64027, 2481, 
	265743, 43832, 113390, 37302, 255474, 37930, 97974, 50085, 30520, 53249, 
	9669, 19077, 8698, 2527, 128954, 82225, 99537, 12948, 17041, 13229, 
	29913, 64236, 23177, 19638, 19284, 10734, 27651, 13643, 159984, 32055, 
	37072, 31326, 10133, 44272, 29103, 73198, 45518, 20431, 78646, 124136, 
	32405, 437928, 83075, 82835, 26139, 78349, 100309, 9348, 15118, 87994, 
	193260, 64148, 33269, 195726, 246297, 37216, 41421, 28982, 33583, 61761, 
	96577, 44402, 34973, 39442, 117641, 46791, 109820, 157964, 38233, 171932, 
	15506, 41300, 25142, 121457, 22915, 15401, 13117, 100395, 12393, 29844, 
	53631, 37621, 1839, 189856, 17671, 123831, 15265, 15950, 105008, 11045, 
	104919, 38181, 17917, 63593, 39498, 162541, 70285, 4493, 11988, 12130, 
	172831, 10841, 129004, 16611, 7730, 43699, 53784, 31584, 52960, 109150, 
	106389, 99529, 24110, 37749, 81189, 16295, 26336, 95104, 24062, 96811, 
	40907, 23483, 69798, 9821, 70370, 64157, 13233, 2964, 3340, 57980, 
	48499, 21648, 139413, 25614, 101920, 68001, 37475, 29964, 281688, 82636, 
	63402, 166077, 72512, 55853, 54834, 16844, 5344, 8213, 16750, 92777, 
	12901, 186291, 5233, 47559, 90392, 14283, 4655, 30827, 4369, 296, 
	2974, 42787, 74125, 70769, 15561, 68557, 33641, 2651, 39873, 14350, 
	123309, 44272, 2756, 54170, 96893, 7449, 22590, 100866, 38523, 37962, 
	310183, 75467, 5002, 113432, 20139, 75366, 10223, 9626, 2902, 93067, 
	353924, 54803, 60636, 56704, 57029, 20109, 8670, 47684, 39459, 9580, 
	24221, 3555, 57299, 208521, 47178, 5775, 40642, 92909, 37452, 83853, 
	24890, 20253, 143420, 91900, 48127, 7521, 38410, 28417, 46160, 4674, 
	22944, 36422, 97612, 249013, 30091, 32729, 5488, 25066, 74187, 174721, 
	1832, 66999, 71705, 103764, 206111, 22958, 4817, 211037, 87626, 80, 
	119152, 44719, 27825, 80881, 7967, 85978, 271024, 68422, 99387, 68697, 
	32927, 8819, 54330, 17518, 115917, 69265, 102676, 55696, 78218, 13548, 
	17255, 30874, 17063, 46053, 60024, 87861, 24615, 102278, 247716, 4240, 
	70849, 29186, 21347, 57262, 51874, 3913, 29282, 12120, 8091, 14243, 
	124224, 117256, 66180, 11593, 11276, 34718, 45992, 66520, 82240, 100160, 
	24972, 1126, 111403, 107584, 156249, 87170, 66076, 314707, 23090, 476, 
	246929, 64000, 24746, 59114, 112717, 34290, 239, 111026, 65364, 25458, 
	71441, 9048, 94023, 19363, 18505, 5804, 29281, 35474, 6371, 125763, 
	75263, 63272, 18898, 229349, 34559, 23567, 36232, 373, 30958, 18740, 
	160696, 114465, 21754, 9391, 41847, 131575, 14104, 44005, 182942, 98576, 
	65509, 17740, 71097, 4138, 1315, 31749, 18369, 198943, 103419, 35616, 
	39803, 144747, 30976, 14757, 199256, 3986, 257700, 41095, 12762, 7481, 
	37102, 40313, 16581, 12104, 67320, 4649, 27549, 9637, 19414, 40547, 
	264665, 91462, 3739, 14641, 794, 30649, 140639, 20127, 114792, 152420, 
	6634, 121923, 59142, 208242, 81643, 249953, 43306, 4012, 7416, 36676, 
	79703, 11628, 176955, 45798, 50159, 19625, 1048, 110544, 24615, 26470, 
	11756, 136840, 82172, 9582, 9893, 129496, 56324, 15205, 10333, 11737, 
	73620, 9843, 128158, 86644, 36734, 29757, 11255, 120120, 266883, 3488, 
	94172, 39076, 24049, 4947, 22443, 135471,
};

BOOST_FIXTURE_TEST_SUITE(claimtrie_tests, TestingSetup)

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

CMutableTransaction BuildTransaction(const CTransaction& prev, uint32_t prevout=0, unsigned int numOutputs=1)
{
    return BuildTransaction(CMutableTransaction(prev), prevout, numOutputs);
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
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
    pblock->vtx[0] = CTransaction(txCoinbase);
    pblock->hashMerkleRoot = pblock->ComputeMerkleRoot();
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

bool CreateCoinbases(unsigned int num_coinbases, std::vector<CTransaction>& coinbases, CNoncePrinter* pnp, unsigned int const * nonces, unsigned int& nonce_counter)
{
    CBlockTemplate *pblocktemplate;
    coinbases.clear();
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 1);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 100 + num_coinbases; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, nonces[nonce_counter++]));
        if (coinbases.size() < num_coinbases)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }
    delete pblocktemplate;
    return true;
}

bool CreateBlocks(unsigned int num_blocks, unsigned int num_txs, CNoncePrinter* pnp, unsigned int const * nonces, unsigned int& nonce_counter)
{
    CBlockTemplate *pblocktemplate;
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == num_txs);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < num_blocks; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, pnp, nonces[nonce_counter++]));
    }
    delete pblocktemplate;
    return true;
}

BOOST_AUTO_TEST_CASE(claimtrie_merkle_hash)
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

    BOOST_CHECK(pclaimTrie->empty());

    CClaimTrieCache ntState(pclaimTrie);
    ntState.insertClaimIntoTrie(std::string("test"), CNodeValue(tx1.GetHash(), 0, 50, 100, 200));
    ntState.insertClaimIntoTrie(std::string("test2"), CNodeValue(tx2.GetHash(), 0, 50, 100, 200));

    BOOST_CHECK(pclaimTrie->empty());
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

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState1(pclaimTrie);
    ntState1.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("test2"), tx2.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("tes"), tx4.GetHash(), 0, unused);

    BOOST_CHECK(ntState1.getMerkleHash() == hash0);

    CClaimTrieCache ntState2(pclaimTrie);
    ntState2.insertClaimIntoTrie(std::string("abab"), CNodeValue(tx6.GetHash(), 0, 50, 100, 200));
    ntState2.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);

    BOOST_CHECK(ntState2.getMerkleHash() == hash3);

    ntState2.flush();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash3);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState3(pclaimTrie);
    ntState3.insertClaimIntoTrie(std::string("test"), CNodeValue(tx1.GetHash(), 0, 50, 100, 200));
    BOOST_CHECK(ntState3.getMerkleHash() == hash4);
    ntState3.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash4);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState4(pclaimTrie);
    ntState4.removeClaimFromTrie(std::string("abab"), tx6.GetHash(), 0, unused);
    BOOST_CHECK(ntState4.getMerkleHash() == hash2);
    ntState4.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState5(pclaimTrie);
    ntState5.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);

    BOOST_CHECK(ntState5.getMerkleHash() == hash2);
    ntState5.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState6(pclaimTrie);
    ntState6.insertClaimIntoTrie(std::string("test"), CNodeValue(tx3.GetHash(), 0, 50, 101, 201));

    BOOST_CHECK(ntState6.getMerkleHash() == hash2);
    ntState6.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState7(pclaimTrie);
    ntState7.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("tes"), tx4.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("test2"), tx2.GetHash(), 0, unused);
    
    BOOST_CHECK(ntState7.getMerkleHash() == hash0);
    ntState7.flush();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pclaimTrie->checkConsistency());
}

BOOST_AUTO_TEST_CASE(claimtrie_insert_update_claim)
{
    unsigned int block_counter = 0;
    //CNoncePrinter noncePrinter;
    CNoncePrinter* pnp = NULL;//&noncePrinter;
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);
    
    LOCK(cs_main);
    //Checkpoints::fEnabled = false;

    std::string sName1("atest");
    std::string sName2("btest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName1(sName1.begin(), sName1.end());
    std::vector<unsigned char> vchName2(sName2.begin(), sName2.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(4, coinbases, pnp, insert_nonces, block_counter));

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    
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
    CMutableTransaction tx8 = BuildTransaction(tx3, 0, 2);
    tx8.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx8.vout[0].nValue = tx8.vout[0].nValue - 1;
    tx8.vout[1].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CNodeValue val;

    std::vector<uint256> blocks_to_invalidate;
    
    // Verify updates to the best claim get inserted immediately, and others don't.
    
    // Put tx1, tx2, and tx7 into the blockchain, and then advance 100 blocks to put them in the trie
    
    AddToMempool(tx1);
    AddToMempool(tx2);
    AddToMempool(tx7);

    BOOST_CHECK(CreateBlocks(1, 4, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx7.GetHash()));

    BOOST_CHECK(CreateBlocks(99, 1, pnp, insert_nonces, block_counter));
    
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName2, val));
    
    BOOST_CHECK(CreateBlocks(1, 1, pnp, insert_nonces, block_counter));
    
    // Verify tx1 and tx2 are in the trie

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Verify tx7 is also in the trie

    BOOST_CHECK(pclaimTrie->haveClaim(sName1, tx7.GetHash(), 0));

    // Spend tx1 with tx3, tx2 with tx4, and put in tx5.
    
    AddToMempool(tx3);
    AddToMempool(tx4);
    AddToMempool(tx5);

    BOOST_CHECK(CreateBlocks(1, 4, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    // Verify tx1, tx2, and tx5 are not in the trie, but tx3 is in the trie.

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName2, val));

    // Roll back the last block, make sure tx1 and tx2 are put back in the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Roll all the way back, make sure all txs are out of the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName2, val));
    mempool.clear();
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test undoing a claim before the claim gets into the trie

    // Put tx1 in the chain, and then undo that block.
    
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // Make sure it's not in the queue

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing a claim that has gotten into the trie

    // Put tx1 in the chain, and then advance until it's in the trie

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(99, 1, pnp, insert_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, insert_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // Remove it from the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spend which involves a claim just inserted into the queue

    // Immediately spend tx2 with tx4, verify nothing gets put in the trie

    AddToMempool(tx2);
    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 3, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spend which involves a claim inserted into the queue by a previous block
    
    // Put tx2 into the chain, and then advance a few blocks but not far enough for it to get into the trie

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(49, 1, pnp, insert_nonces, block_counter));
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    AddToMempool(tx4);
    
    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(50, 1, pnp, insert_nonces, block_counter));
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Undo spending tx2 with tx4, and then advance and verify tx2 is inserted into the trie when it should be

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    mempool.clear();

    BOOST_CHECK(CreateBlocks(50, 1, pnp, insert_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, insert_nonces, block_counter));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Test undoing a spend which involves a claim in the trie

    // spend tx2, which is in the trie, with tx4
    
    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // undo spending tx2 with tx4, and verify tx2 is back in the trie
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // roll back to the beginning
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spent update which updated a claim still in the queue

    // Create the original claim (tx1)

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // move forward some, but not far enough for the claim to get into the trie

    BOOST_CHECK(CreateBlocks(49, 1, pnp, insert_nonces, block_counter));
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // update the original claim (tx3 spends tx1)

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // spend the update (tx6 spends tx3)

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // undo spending the update (undo tx6 spending tx3)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    mempool.clear();

    // make sure the update (tx3) still goes into effect in 100 blocks

    BOOST_CHECK(CreateBlocks(99, 1, pnp, insert_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, insert_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // undo updating the original claim (tx3 spends tx1)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing an spent update which updated the best claim to a name

    // move forward until the original claim is inserted into the trie

    BOOST_CHECK(CreateBlocks(50, 1, pnp, insert_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, insert_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // update the original claim (tx3 spends tx1)
    
    AddToMempool(tx3);
    
    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    
    // spend the update (tx6 spends tx3)
    
    AddToMempool(tx6);
    
    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    
    // undo spending the update (undo tx6 spending tx3)
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());

    // Test having two updates to a claim in the same transaction

    // Add both txouts (tx8 spends tx3)
    
    AddToMempool(tx8);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, insert_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    // ensure txout 0 made it into the trie and txout 1 did not
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx8.GetHash() && val.nOut == 0);

    // roll forward until tx8 output 1 gets into the trie

    BOOST_CHECK(CreateBlocks(99, 1, pnp, insert_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, insert_nonces, block_counter));

    // ensure txout 1 made it into the trie and is now in control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx8.GetHash() && val.nOut == 1);

    // roll back to before tx8

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    //noncePrinter.printNonces();
}

BOOST_AUTO_TEST_CASE(claimtrie_claim_expiration)
{
    unsigned int block_counter = 0;
    //CNoncePrinter noncePrinter;
    CNoncePrinter* pnp = NULL;//&noncePrinter;
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue("testa");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue(sValue.begin(), sValue.end());

    std::vector<CTransaction> coinbases;
   
    BOOST_CHECK(CreateCoinbases(2, coinbases, pnp, expire_nonces, block_counter));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(tx1);
    tx2.vout[0].scriptPubKey = CScript() << OP_TRUE;

    std::vector<uint256> blocks_to_invalidate;
    // set expiration time to 100 blocks after the block becomes valid. (more correctly, 200 blocks after the block is created)

    pclaimTrie->setExpirationTime(200);

    // create a claim. verify no expiration event has been scheduled.

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the claim is valid. verify the expiration event is scheduled.

    BOOST_CHECK(CreateBlocks(99, 1, pnp, expire_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->empty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.
    
    BOOST_CHECK(CreateBlocks(99, 1, pnp, expire_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    
    // roll forward a bit and then roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.

    BOOST_CHECK(CreateBlocks(100, 1, pnp, expire_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    
    // advance until the expiration event occurs. verify the expiration event occurs on time.
    
    BOOST_CHECK(CreateBlocks(1, 1, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    
    // roll back to before the claim is valid. verify the claim is removed but the expiration event still exists.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the claim is valid again. verify the expiration event is scheduled.

    BOOST_CHECK(CreateBlocks(1, 1, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.

    BOOST_CHECK(CreateBlocks(50, 1, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    BOOST_CHECK(CreateBlocks(49, 1, pnp, expire_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the expiration event is scheduled.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll back some.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // spend the claim. verify the expiration event is removed.
    
    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    
    // roll back the spend. verify the expiration event is returned.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
   
    mempool.clear();
    
    // advance until the expiration event occurs. verify the event occurs on time.
    
    BOOST_CHECK(CreateBlocks(50, 1, pnp, expire_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // spend the expired claim

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // undo the spend. verify everything remains empty.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    mempool.clear();
    
    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // verify the expiration event happens at the right time again

    BOOST_CHECK(CreateBlocks(1, 1, pnp, expire_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify it gets reinserted and expiration gets scheduled.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll back to before the claim is valid. verify the claim is removed but the expiration event is not.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(blocks_to_invalidate.empty());

    //noncePrinter.printNonces();
}

BOOST_AUTO_TEST_CASE(claimtrie_supporting_claims)
{
    unsigned int block_counter = 0;
    //CNoncePrinter noncePrinter;
    CNoncePrinter* pnp = NULL;//&noncePrinter;
    
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases, pnp, support_nonces1, block_counter));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 100000000;

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 500000000;

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    uint256 tx1Hash = tx1.GetHash();
    std::vector<unsigned char> vchTx1Hash(tx1Hash.begin(), tx1Hash.end());
    std::vector<unsigned char> vchSupportnOut = uint32_t_to_vch(0);
    tx3.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchName << vchTx1Hash << vchSupportnOut << OP_2DROP << OP_2DROP << OP_TRUE;
    tx3.vout[0].nValue = 500000000;

    CMutableTransaction tx4 = BuildTransaction(tx1);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx5 = BuildTransaction(tx2);
    tx5.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CNodeValue val;
    std::vector<uint256> blocks_to_invalidate;

    // Test 1: create 1 LBC claim (tx1), create 5 LBC support (tx3), create 5 LBC claim (tx2)
    // Verify that tx1 retains control throughout
    // spend tx3, verify that tx2 gains control
    // roll back to before tx3 is valid
    // advance until tx3 and tx2 are valid, verify tx1 retains control
    // spend tx3, verify tx2 gains control
    // roll back to before tx3 is spent, verify tx1 gains control
    // roll back to before tx2 is valid, spend tx3
    // advance to tx2 valid, verify tx2 gains control
    // roll back to before tx3 is valid, spend tx3
    // advance to tx2 valid, verify tx2 gains control
    // roll back to insertion of tx3, and don't insert it
    // insert tx2, advance until it is valid, verify tx2 gains control

    // Put tx1 in the blockchain

    AddToMempool(tx1);
    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces1, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces1, block_counter));

    // Put tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces1, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces1, block_counter));

    // Put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces1, block_counter));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    
    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance until tx3 is valid
    
    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces1, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // spend tx3

    AddToMempool(tx6);
    
    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces1, block_counter));

    // verify tx2 gains control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());
    
    // roll back to before tx3 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    // advance until tx3 is valid
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces1, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces1, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // spend tx3
    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces1, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    // verify tx2 gains control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // undo spending tx3

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // verify tx1 has control again

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // roll back to before tx2 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces1, block_counter));

    // Verify tx2 gains control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll back to before tx3 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll back to before tx3 is inserted
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(20, 1, pnp, support_nonces1, block_counter));

    // Put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces1, block_counter));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    
    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(39, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces1, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    //noncePrinter.printNonces();
}

BOOST_AUTO_TEST_CASE(claimtrie_supporting_claims2)
{
    unsigned int block_counter = 0;
    //CNoncePrinter noncePrinter;
    CNoncePrinter* pnp = NULL;//&noncePrinter;
    
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases, pnp, support_nonces2, block_counter));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 100000000;

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 500000000;

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    uint256 tx1Hash = tx1.GetHash();
    std::vector<unsigned char> vchTx1Hash(tx1Hash.begin(), tx1Hash.end());
    std::vector<unsigned char> vchSupportnOut = uint32_t_to_vch(0);
    tx3.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchName << vchTx1Hash << vchSupportnOut << OP_2DROP << OP_2DROP << OP_TRUE;
    tx3.vout[0].nValue = 500000000;

    CMutableTransaction tx4 = BuildTransaction(tx1);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx5 = BuildTransaction(tx2);
    tx5.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CNodeValue val;
    std::vector<uint256> blocks_to_invalidate;
    
    // Test 2: create 1 LBC claim (tx1), create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 loses control when tx2 becomes valid, and then tx1 gains control when tx3 becomes valid
    // Then, verify that tx2 regains control when A) tx3 is spent and B) tx3 is undone
    
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // advance 20 blocks
    
    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces2, block_counter));

    // put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces2, block_counter));

    // put tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // undo spend

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // roll back to before tx3 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 4: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 retains control throughout

    // put tx1 into the blockchain

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(99, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // advance some, insert tx3, should be immediately valid

    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // advance until tx2 is valid, verify tx1 retains control

    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 5: create 5 LBC claim (tx2), wait till valid, create 1 LBC claim (tx1), create 5 LBC support (tx3)
    // Verify that tx2 retains control until support becomes valid

    // insert tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(99, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // insert tx1 into the block chain

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance some
    
    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));

    // insert tx3 into the block chain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));
    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));
    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());


    // Test 6: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3), spend tx1
    // Verify that tx1 retains control until it is spent

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(99, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // insert tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance some, insert tx3

    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // spend tx1

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());
    
    // undo spend of tx1

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // Test 7: create 1 LBC claim (tx1), wait till valid, create 5 LBC support (tx3), spend tx1
    // Verify name trie is empty

    // insert tx1 into blockchain
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(99, 1, pnp, support_nonces2, block_counter));
    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // insert tx3 into blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // spent tx1

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // Test 8: create 1 LBC claim (tx1), create 5 LBC support (tx3), wait till tx1 valid, spend tx1, wait till tx3 valid
    // Verify name trie is empty

    // insert tx1 into the blockchain

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // move forward a bit

    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));

    // insert tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    
    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(49, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    
    // spend tx1

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces2, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    
    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(48, 1, pnp, support_nonces2, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces2, block_counter));
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    //noncePrinter.printNonces();
}

BOOST_AUTO_TEST_CASE(claimtrienode_serialize_unserialize)
{
    CDataStream ss(SER_DISK, 0);

    CClaimTrieNode n1;
    CClaimTrieNode n2;
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
