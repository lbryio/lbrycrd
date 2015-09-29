// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"
#include "miner.h"
#include "claimtrie.h"
#include "coins.h"
#include "streams.h"
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

const unsigned int support_nonces[] = {
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
	36089, 89042, 1064, 57622, 18277, 30812, 392721, 68449, 21958, 59353, 
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
	230626, 192876, 152661, 83303, 12403, 48000, 322, 36098, 216060, 261073, 
	10127, 40078, 13820, 37595, 2465, 67578, 8074, 17069, 23001, 75590, 
	59540, 38500, 81671, 83017, 21630, 42072, 87988, 34685, 54463, 73723, 
	64583, 11708, 27819, 60914, 44671, 73532, 481251, 50437, 51482, 140164, 
	17802, 52420, 18605, 39313, 5815, 130397, 47241, 41700, 73784, 38254, 
	31816, 81033, 63873, 61180, 73597, 31012, 46596, 34360, 16076, 3553, 
	19667, 70678, 76463, 14007, 6695, 34346, 177277, 82740, 10695, 195656, 
	199473, 19061, 12235, 118857, 5890, 158834, 14991, 9908, 40669, 76476, 
	5798, 56010, 12434, 136848, 44171, 33686, 38022, 85052, 88529, 96055, 
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
    CNoncePrinter noncePrinter;
    CNoncePrinter* pnp = &noncePrinter;
    
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases, pnp, support_nonces, block_counter));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 100000000;

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 500000000;

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    uint256 tx1Hash = tx1.GetHash();
    std::vector<unsigned char> vchTx1Hash(tx1Hash.begin(), tx1Hash.end());
    tx3.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchName << vchTx1Hash << CScriptNum(0) << OP_2DROP << OP_2DROP << OP_TRUE;
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
    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    // Put tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    // Put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    
    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance until tx3 is valid
    
    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // spend tx3

    AddToMempool(tx6);
    
    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));

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

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // spend tx3
    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));
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

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));

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

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));

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

    BOOST_CHECK(CreateBlocks(20, 1, pnp, support_nonces, block_counter));

    // Put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    
    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(39, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // Test 2: create 1 LBC claim (tx1), create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 loses control when tx2 becomes valid, and then tx1 gains control when tx3 becomes valid
    // Then, verify that tx2 regains control when A) tx3 is spent and B) tx3 is undone
    
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // advance 20 blocks
    
    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    // put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    // put tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2, pnp, support_nonces, block_counter));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(19, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(CreateBlocks(1, 1, pnp, support_nonces, block_counter));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // Test 4: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 retains control throughout

    // Test 5: create 5 LBC claim (tx2), wait till valid, create 1 LBC claim (tx1), create 5 LBC support (tx3)
    // Verify that tx2 retains control until support becomes valid

    // Test 6: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3), spend tx1
    // Verify that tx1 retains control until it is spent

    // Test 7: create 1 LBC claim (tx1), wait till valid, create 5 LBC support (tx3), spend tx1
    // Verify name trie is empty

    // Test 8: create 1 LBC claim (tx1), create 5 LBC support (tx3), wait till tx1 valid, spend tx1, wait till tx3 valid
    // Verify name trie is empty
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
