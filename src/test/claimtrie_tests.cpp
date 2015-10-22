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
	154662, 37101, 39016, 32108, 11193, 167425, 41041, 18322, 52212, 51840, 
	42878, 91589, 61917, 124810, 27102, 2216, 21835, 126406, 39437, 17288, 
	34800, 111796, 707, 36455, 153492, 159933, 12398, 67544, 37082, 30098, 
	229205, 47964, 5544, 3483, 85833, 98896, 114927, 4548, 5466, 339131, 
	53091, 12742, 120717, 16555, 66390, 60497, 146946, 179155, 101302, 25237, 
	7373, 29939, 192727, 139469, 100098, 7412, 10109, 108590, 85040, 57723, 
	205366, 14108, 1675, 104092, 4531, 6625, 194140, 108134, 51179, 286475, 
	57364, 35435, 101339, 49842, 170756, 74109, 167787, 74888, 13734, 2200, 
	51025, 20673, 114840, 226387, 45924, 81994, 15964, 72198, 137478, 189006, 
	5563, 76228, 112664, 6093, 22526, 218231, 28274, 62240, 834, 3040, 
	11374, 44162, 32366, 28706, 27518, 73387, 74858, 13841, 
};

const unsigned int expire_nonces[] = {
	53643, 193025, 37619, 252203, 58420, 37206, 120531, 38736, 98682, 46851, 
	5408, 42419, 79305, 42088, 27227, 38681, 10473, 68404, 13386, 148340, 
	27265, 14930, 23512, 259139, 52524, 139580, 15274, 107872, 13512, 39058, 
	6528, 171375, 100432, 20647, 38238, 41994, 32188, 20306, 37666, 102447, 
	122325, 24994, 13778, 12889, 14487, 5961, 212456, 5949, 14945, 72000, 
	34139, 52284, 72275, 39792, 3626, 47870, 26350, 2030, 21205, 242795, 
	6922, 42897, 49239, 36965, 29472, 47434, 39801, 7848, 107087, 56181, 
	11574, 218603, 921, 62918, 87550, 5039, 199191, 11595, 151114, 69613, 
	200746, 93161, 24752, 54040, 30086, 243689, 4355, 148401, 23334, 22731, 
	67536, 4909, 129386, 17413, 48047, 59554, 80856, 112253, 116605, 49616, 
	28509, 124700, 81689, 20237, 45435, 65531, 88501, 69505, 1910, 2339, 
	105547, 3571, 92337, 40606, 19594, 65255, 171478, 180842, 42044, 165883, 
	13358, 31680, 36101, 172555, 96773, 72536, 155350, 2098, 11005, 150888, 
	23162, 238079, 8065, 88390, 59240, 159426, 18883, 100712, 50303, 12778, 
	4122, 39015, 87028, 21859, 204734, 125392, 7046, 101177, 30313, 38983, 
	33926, 12411, 89317, 13614, 32803, 20403, 9974, 155161, 126861, 138228, 
	96019, 41401, 7382, 276438, 241196, 87614, 19478, 50243, 29382, 27322, 
	50149, 12226, 72311, 261323, 148422, 33798, 19257, 159967, 35923, 1871, 
	35370, 1934, 20297, 67350, 39297, 37949, 27448, 11718, 99752, 78981, 
	6827, 9146, 54521, 11337, 32495, 131559, 74671, 18399, 9176, 23039, 
	68035, 35216, 10352, 48005, 57711, 4111, 10910, 3728, 42934, 148284, 
	17258, 25026, 21188, 74496, 85191, 42063, 88363, 50770, 6496, 96991, 
	18379, 166816, 51923, 11159, 46012, 40566, 191245, 77859, 10582, 39886, 
	14617, 87189, 10621, 1924, 82382, 17764, 6480, 57891, 34821, 2814, 
	54541, 73597, 97789, 127407, 32318, 12967, 29903, 11871, 96127, 29539, 
	43608, 8382, 9974, 13281, 201100, 8637, 310817, 49145, 46113, 6755, 
	140141, 237272, 101425, 275710, 35425, 107336, 21626, 24303, 67533, 97938, 
	38556, 315214, 12328, 322, 51291, 15382, 7078, 111, 11374, 13528, 
	23632, 114220, 5723, 34296, 86986, 16045, 522, 22529, 5611, 73259, 
	48757, 39981, 58614, 69598, 172362, 61830, 38832, 77165, 17464, 229210, 
	146635, 100205, 72180, 115848, 65228, 47877, 27028, 136614, 37698, 6976, 
	16142, 114525, 149787, 14563, 26711, 75974, 1473, 80188, 115058, 144729, 
	39907, 40755, 30276, 46183, 119711, 206462, 17425, 103303, 2199, 57205, 
	226714, 57224, 110498, 60932, 93610, 129, 130557, 59604, 22063, 43375, 
	11610, 28350, 7582, 79781, 10865, 43149, 96470, 36001, 192437, 179354, 
	10228, 89266, 7910, 156039, 267310, 13344, 93112, 27293, 42582, 55182, 
	27195, 17225, 93669, 102846, 64936, 1136, 140, 73174, 8659, 112737, 
	131488, 53985, 80662, 176405, 87433, 170967, 10978, 185759, 199916, 34024, 
	7838, 5129, 3591, 8126, 19177, 33500, 143216, 39321, 16370, 143905, 
	2679, 51503, 92834, 19531, 33340, 5086, 70009, 4983, 74213, 68851, 
	26319, 10016, 55917, 137039, 20227, 9087, 6602, 207466, 1920, 14902, 
	8778, 112930, 6476, 39473, 47094, 4146, 46644, 230231, 59400, 8385, 
	80074, 92678, 13163, 440, 61457, 65142, 1264, 47993, 28298, 22303, 
	102585, 84609, 62724, 32058, 4264, 12953, 66000, 20420, 5845, 9863, 
	38763, 33772, 92168, 68659, 121676, 16753, 56006, 94837, 72432, 42723, 
	10070, 18181, 27435, 55469, 5014, 6212, 32777, 13640, 155058, 4801, 
	2172, 22607, 22045, 337783, 15163, 14092, 70761, 45246, 57947, 8256, 
	21857, 57639, 69802, 37692, 75350, 108166, 110128, 10297, 202304, 277189, 
	100368, 258936, 31115, 6587, 41871, 122116, 49827, 78331, 38975, 159839, 
	16620, 8790, 25848, 32898, 21040, 381409, 33867, 198668, 25577, 50833, 
	1563, 66657, 46516, 42621, 22630, 33949, 46949, 17531, 7166, 61699, 
	32573, 14976, 62930, 117242, 16964, 87579, 33119, 235295, 54355, 60343, 
	22289, 58048, 1946, 36960, 25046, 5308, 205643, 220624, 64664, 68716, 
	97214, 40261, 141321, 99397, 132590, 14605, 58838, 62340, 44877, 91900, 
	3790, 25997, 1795, 11827, 97922, 93, 179423, 350, 10327, 40879, 
	22343, 31416, 138676, 3672, 41575, 77002, 199599, 59124, 4381, 
};

const unsigned int support_nonces1[] = {
	105701, 115584, 37683, 27712, 42977, 51687, 132104, 219642, 77841, 55531, 
	13564, 117419, 73277, 153293, 142955, 72110, 37463, 62119, 78019, 43317, 
	10408, 52672, 284680, 230150, 21623, 48803, 16417, 55640, 129049, 116488, 
	41627, 7494, 78035, 1595, 22143, 301, 89697, 25699, 4234, 71440, 
	7401, 58869, 53932, 29626, 1163, 16368, 117353, 196807, 40560, 75864, 
	86242, 164842, 261952, 42966, 31269, 151460, 193885, 120629, 28690, 103892, 
	95195, 16868, 64307, 6336, 35715, 5468, 55442, 4855, 1558, 47981, 
	1673, 12417, 87010, 57350, 66760, 347, 136748, 23558, 156370, 8112, 
	32751, 67399, 78430, 62284, 38879, 63197, 41810, 47568, 2194, 20021, 
	55230, 65910, 11530, 7441, 110596, 108561, 71692, 43935, 151361, 79724, 
	99792, 107989, 82596, 9169, 291679, 112377, 41344, 185047, 7018, 5951, 
	88404, 140428, 87970, 81004, 152441, 36684, 173721, 3349, 3175, 252786, 
	32720, 40948, 76930, 20858, 43320, 45434, 68488, 164113, 20916, 239879, 
	40956, 111839, 68933, 74051, 192633, 25137, 97047, 72680, 153778, 109034, 
	43666, 16939, 18176, 133058, 32517, 46410, 194708, 40105, 58360, 137048, 
	10313, 165416, 20483, 7205, 5507, 19765, 3457, 154415, 48932, 82199, 
	160148, 23694, 55796, 78716, 70039, 164, 41686, 53995, 54996, 67472, 
	47392, 17816, 214640, 22687, 169977, 101026, 9873, 49577, 10778, 125898, 
	76370, 34024, 177655, 93657, 24720, 31059, 17496, 111609, 281936, 16678, 
	7952, 78319, 26740, 34426, 43900, 59040, 93664, 43267, 70771, 4339, 
	161842, 14987, 68264, 16, 36136, 80695, 13263, 90082, 857, 107417, 
	161366, 1416, 14466, 10056, 23907, 74171, 5261, 240414, 15976, 56496, 
	50007, 93917, 101335, 33195, 5902, 26325, 69080, 157373, 51960, 691, 
	54830, 67430, 33423, 127739, 9219, 62709, 495080, 227830, 73772, 13165, 
	3613, 117489, 4320, 114186, 132594, 69, 46875, 33889, 172546, 15312, 
	21267, 85199, 7134, 67319, 76992, 21812, 51259, 9249, 9862, 24484, 
	59952, 75454, 75256, 2651, 39566, 75408, 38682, 35976, 27900, 43089, 
	156885, 43435, 193627, 83950, 5709, 60268, 92811, 13408, 15480, 58406, 
	215450, 28785, 19379, 10100, 44302, 19849, 74044, 26939, 37505, 61336, 
	20033, 19624, 63951, 6755, 18613, 43297, 180012, 66684, 6631, 121649, 
	22531, 320516, 74811, 59326, 17935, 39781, 137162, 44383, 33226, 114852, 
	181180, 67566, 157014, 13412, 166344, 47237, 2946, 1812, 122065, 28047, 
	46154, 219650, 86082, 70152, 104421, 159526, 78937, 87378, 29769, 236780, 
	23349, 89921, 115223, 27581, 14524, 59617, 83790, 54662, 33948, 86135, 
	2294, 17568, 6718, 28464, 19443, 48403, 49226, 116460, 41971, 145968, 
	18998, 18336, 4439, 17381, 31806, 174135, 8843, 5821, 59615, 70414, 
	8139, 92130, 292188, 27433, 32219, 3543, 264777, 64492, 56545, 2022, 
	84938, 134120, 31399, 47938, 2636, 78275, 95149, 3045, 23538, 37556, 
	7601, 14137, 11467, 106593, 48554, 155380, 139237, 20564, 17083, 13782, 
	259910, 28778, 14579, 23222, 217354, 106378, 31622, 138311, 62060, 231925, 
	137370, 99259, 12165, 71289, 288569, 138298, 72384, 51470, 7769, 58315, 
};

const unsigned int support_nonces2[] = {
	48702, 44910, 140964, 17704, 11556, 21180, 29805, 90484, 193777, 16876, 
	15155, 27884, 78520, 659, 92784, 99385, 69875, 18357, 13829, 87382, 
	46608, 65607, 65131, 168763, 53739, 27749, 96759, 15690, 2153, 67153, 
	13489, 139244, 54725, 26897, 93614, 18265, 6707, 12046, 27070, 75341, 
	139417, 12032, 134983, 58875, 2878, 75451, 58147, 3217, 79206, 85487, 
	18664, 41326, 1626, 56088, 47222, 12817, 40648, 49519, 63781, 75246, 
	10883, 263162, 8502, 110769, 26554, 104794, 1160, 35622, 1581, 21486, 
	23668, 31824, 28294, 41480, 109983, 5951, 100510, 20950, 35565, 47013, 
	41826, 91009, 52933, 58867, 62875, 84031, 131209, 7100, 81796, 292517, 
	49363, 59229, 28120, 5635, 83082, 19464, 6361, 11129, 111566, 20012, 
	17332, 118951, 63053, 1140, 52805, 55065, 71125, 13182, 112795, 81320, 
	81635, 191166, 66330, 36626, 94722, 52716, 133306, 20991, 64508, 34170, 
	88659, 9779, 70051, 123365, 158677, 142215, 165612, 176285, 5277, 69969, 
	123460, 4739, 25210, 1181, 22451, 147063, 45230, 183496, 50047, 142425, 
	90173, 10472, 97468, 2218, 111455, 212848, 2916, 18721, 30825, 190739, 
	113909, 24856, 8440, 86811, 211, 29718, 17486, 59240, 46915, 20444, 
	76005, 12477, 16029, 16541, 17049, 3641, 51405, 22015, 102809, 82073, 
	79420, 20290, 178940, 16336, 11521, 80562, 151537, 24375, 14822, 89007, 
	168314, 106790, 190447, 9747, 71739, 73323, 223536, 43707, 6166, 53859, 
	77193, 19913, 180298, 148, 20032, 43052, 68174, 63836, 76626, 45639, 
	136164, 15480, 66532, 189984, 70371, 46701, 87073, 101098, 13573, 32446, 
	40411, 3277, 5337, 4480, 33121, 31322, 16519, 46549, 8561, 9329, 
	15755, 57765, 37039, 52216, 170054, 23913, 3224, 22095, 43992, 66225, 
	764, 75691, 35599, 5417, 269692, 86253, 90212, 26251, 8874, 16060, 
	27117, 33737, 203711, 116599, 19914, 62543, 2856, 308138, 9975, 5428, 
	39726, 1759, 24200, 6970, 154491, 59163, 144476, 46342, 64023, 78238, 
	71701, 52740, 30958, 122696, 6737, 210973, 32609, 19114, 58624, 92640, 
	12948, 11073, 16901, 25895, 21842, 9027, 60460, 33030, 67370, 44786, 
	113286, 85555, 29815, 37009, 75025, 23515, 65613, 104071, 7267, 63431, 
	96262, 8044, 61330, 19289, 78340, 12648, 198796, 175488, 22506, 31865, 
	83995, 94606, 113201, 11202, 10724, 33568, 54055, 43569, 191502, 5594, 
	24091, 27495, 6129, 128914, 51810, 33524, 29688, 35199, 39280, 108535, 
	1736, 70288, 10865, 5106, 185582, 82878, 955, 3329, 2251, 138742, 
	93783, 74610, 35624, 98287, 67057, 8956, 3387, 48018, 6227, 100255, 
	36146, 55132, 49590, 11123, 158871, 457652, 57564, 50630, 4577, 2105, 
	44065, 14316, 105841, 120794, 1948, 2004, 12254, 126543, 7061, 26528, 
	45784, 70172, 44564, 143614, 39200, 75869, 14353, 127113, 70557, 38980, 
	63364, 14319, 18884, 256, 44183, 16794, 45418, 126627, 31531, 60619, 
	16674, 99942, 16293, 4961, 57609, 247725, 102578, 76154, 4376, 74861, 
	27560, 14614, 158129, 126472, 79987, 20773, 48122, 116607, 39308, 150466, 
	64570, 49849, 46186, 23349, 80633, 4727, 45020, 44244, 18138, 9984, 
	12649, 8930, 343846, 8772, 85447, 20913, 6850, 7148, 35165, 6243, 
	80328, 15660, 17966, 103914, 37229, 75098, 32367, 137670, 30573, 13861, 
	99470, 331362, 16339, 294785, 944, 180161, 354424, 29493, 90717, 76017, 
	157363, 94801, 41689, 285734, 11149, 2512, 17019, 86324, 11610, 111098, 
	198344, 10291, 83710, 39609, 33945, 11683, 93409, 28102, 20296, 64664, 
	1384, 23191, 168994, 31248, 81598, 179948, 1255, 7520, 8735, 23643, 
	27822, 136323, 25917, 18533, 105920, 50395, 98221, 2001, 46451, 191395, 
	12709, 5158, 74971, 76936, 104398, 11660, 55449, 208248, 31366, 73869, 
	66816, 163398, 195525, 17431, 22347, 32792, 83361, 66235, 19852, 13907, 
	52216, 40466, 9592, 110002, 10990, 71981, 87843, 271672, 20693, 26582, 
	21839, 250793, 18064, 18993, 45669, 26660, 109817, 48143, 96317, 14468, 
	21931, 10907, 367206, 56636, 42977, 84096, 78926, 50284, 151140, 16814, 
	1581, 46599, 18978, 28613, 165947, 1030, 95786, 49411, 161086, 52389, 
	221160, 8204, 32467, 24016, 462, 12787, 116808, 9401, 7582, 154849, 
	14292, 101912, 2662, 36954, 47938, 84462, 119556, 63212, 15535, 112705, 
	11959, 12403, 226703, 60436, 48242, 180368, 20683, 13789, 38857, 72504, 
	62740, 27778, 16689, 126445, 63668, 135369, 16489, 91265, 87614, 256543, 
	31608, 63165, 8084, 48884, 60285, 233181, 133145, 67325, 38060, 82702, 
	274112, 1546, 17121, 72008, 63213, 66714, 4427, 26378, 293747, 54190, 
	13609, 46687, 14907, 3925, 54506, 89292, 21048, 52230, 80904, 39484, 
	199322, 17743, 68138, 19352, 47055, 144457, 19211, 29628, 69986, 131196, 
	53872, 49387, 152281, 51824, 22041, 33971, 15108, 13337, 162014, 40602, 
	106200, 8292, 19773, 1304, 55807, 70998, 10637, 83313, 122595, 227472, 
	24159, 98863, 39656, 10452, 127345, 6191, 68732, 4303, 94547, 52004, 
	19411, 2513, 229112, 93276, 46105, 94524, 93852, 19074, 7569, 19864, 
	47459, 121864, 50899, 96723, 197798, 91795, 77069, 26481, 147668, 202302, 
	5928, 184427, 12000, 56211, 5622, 24665, 22639, 30812, 169494, 83456, 
	137941, 17169, 2000, 104824, 136832, 71272, 141938, 63227, 28633, 55314, 
	146418, 373502, 191002, 118787, 8980, 15847, 36560, 30751, 212194, 39768, 
	75, 11013, 7055, 104552, 9149, 54963, 2738, 28738, 25418, 48211, 
	39567, 47613, 19398, 177205, 29822, 104444, 115853, 31753, 26932, 48801, 
	111612, 25639, 44335, 94548, 84888, 14268, 10561, 50159, 37092, 19300, 
	37845, 45460, 36084, 59780, 40756, 71487, 60695, 29116, 53714, 24781, 
	58210, 137549, 65394, 34018, 56946, 4416, 1841, 58247, 53607, 32158, 
	88902, 135830, 17041, 39545, 6724, 52312, 53347, 2825, 215441, 74588, 
	19509, 38730, 2412, 127743, 32123, 92704, 179750, 50374, 21157, 222824, 
	54796, 11741, 46491, 45248, 285619, 32196, 106068, 287634, 2688, 118090, 
	82950, 7794, 33720, 2455, 102322, 100087, 169239, 39274, 6861, 66819, 
	114355, 7949, 23741, 54772, 23483, 94184, 14850, 182793, 55775, 57186, 
	6405, 222525, 20364, 69784, 125103, 119738, 30484, 25932, 109801, 71021, 
	67308, 20082, 26806, 30199, 74329, 43277, 65960, 14599, 1792, 104329, 
	25474, 15522, 87555, 60020, 103660, 41229, 115987, 19328, 69277, 102703, 
	53647, 113118, 10548, 19783, 27221, 17091, 14797, 61259, 43178, 28954, 
	123513, 29391, 23953, 70469, 28759, 57824, 19168, 13562, 4571, 26173, 
	60313, 52354, 9731, 33617, 7708, 116302, 145296, 63414, 79680, 598, 
	9047, 47056, 37125, 149483, 5432, 89502, 55348, 172980, 8886, 137667, 
	148918, 23869, 10506, 24099, 238653, 24339, 22464, 7307, 68969, 33856, 
	34296, 203325, 51164, 7833, 109377, 13562, 126128, 159047, 37659, 67068, 
	62885, 17276, 198570, 97716, 125203, 163072, 25870, 101965, 163916, 46804, 
	95510, 41574, 235037, 123980, 9143, 94471, 73153, 11363, 93164, 63362, 
	86526, 24756, 14070, 16924, 83332, 4100, 317190, 285610, 61499, 27712, 
	76315, 4835, 40248, 5447, 12097, 81545, 12833, 37877, 242159, 91197, 
	2425, 26630, 2519, 152473, 65756, 32133, 248062, 4329, 39461, 27326, 
	33558, 98007, 35090, 200461, 19532, 28572, 115567, 10390, 83286, 58626, 
	88522, 12284, 6507, 7279, 66431, 55510, 2911, 25166, 1303, 495978, 
	66969, 161276, 5564, 26862, 122295, 50245, 26803, 227395, 357530, 6129, 
	2874, 24150, 9499, 52916, 14152, 141953, 21624, 22911, 42470, 52998, 
	1080, 37439, 131336, 59290, 38910, 29273, 94703, 8312, 116959, 51277, 
	849, 40262, 190652, 11850, 38769, 87731, 143411, 65908, 50669, 4674, 
	125631, 98358, 39497, 33785, 54496, 44453, 113984, 47609, 2319, 88314, 
	22946, 31369, 99638, 7495, 260385, 116804, 136433, 70265, 98192, 31726, 
	12780, 17332, 3003, 47373, 74798, 26352, 5939, 15375, 8675, 101575, 
	164102, 100976, 101627, 10537, 54993, 31846, 126637, 20135, 158708, 7478, 
	25893, 28672, 66730, 39996, 126228, 20288, 77905, 34495, 19051, 12352, 
	55961, 124354, 58053, 5836, 17005, 3143, 4591, 122833, 14138, 17268, 
	36030, 16341, 56423, 35558, 107746, 52961, 66136, 211036, 46256, 26463, 
	33814, 5837, 34306, 99237, 109457, 20216, 136509, 215181, 26837, 7661, 
	50220, 12966, 89526, 91218, 19980, 120895, 37290, 36883, 18725, 81858, 
	1600, 23014, 255282, 72423, 100392, 1207, 207156, 11959, 62546, 110928, 
	18915, 39838, 11316, 61634, 30352, 166281, 4171, 167586, 23141, 107122, 
	53327, 66841, 15665, 2139, 129790, 152050, 22873, 47846, 46836, 72224, 
	112396, 10779, 81485, 27048, 3607, 15669, 43750, 36080, 54178, 103924, 
	23998, 56178, 247600, 41719, 20168, 123337, 367762, 248158, 71843, 71079, 
	24365, 76, 6585, 64163, 48816, 62945, 91850, 80506, 101195, 93166, 
	102935, 80953, 18790, 93369, 30499, 88326, 
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
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height(), Params().GetConsensus());
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
