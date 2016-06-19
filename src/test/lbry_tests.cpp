#include "arith_uint256.h"
#include "chainparams.h"
#include "lbry.h"
#include "main.h"
#include "test/test_bitcoin.h"
#include <cstdio> 
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(lbry_tests, TestingSetup)

//1 test block 1 difficulty, should be a max retarget
BOOST_AUTO_TEST_CASE(get_block_1_difficulty)
{
    SelectParams(CBaseChainParams::MAIN);
    const Consensus::Params& params = Params().GetConsensus();

    CBlockIndex pindexLast;
    int64_t nFirstBlockTime = 1386475638; 
    
    pindexLast.nHeight = 0;
    pindexLast.nTime = 1386475638; 
    pindexLast.nBits = 0x1f00ffff ;//starting difficulty, also limit 
    unsigned int out = CalculateLbryNextWorkRequired(&pindexLast, nFirstBlockTime, params);
    arith_uint256 a; 
    a.SetCompact(out);
    unsigned int expected = 0x1f00e146;
    BOOST_CHECK_EQUAL(out,expected);

}

// test max retarget (difficulty increase) 
BOOST_AUTO_TEST_CASE(max_retarget)
{
    SelectParams(CBaseChainParams::MAIN);
    const Consensus::Params& params = Params().GetConsensus();

    CBlockIndex pindexLast;
    int64_t nFirstBlockTime = 1386475638;
    
    pindexLast.nHeight = 100;
    pindexLast.nTime = nFirstBlockTime; 
    pindexLast.nBits = 0x1f00a000 ;

    unsigned int out = CalculateLbryNextWorkRequired(&pindexLast, nFirstBlockTime, params);
    arith_uint256 a; 
    a.SetCompact(out); 
    unsigned int expected = 0x1f008ccc;
    BOOST_CHECK_EQUAL(out,expected);


}

// test min retarget (difficulty decrease) 
BOOST_AUTO_TEST_CASE(min_retarget)
{
    SelectParams(CBaseChainParams::MAIN);
    const Consensus::Params& params = Params().GetConsensus();

    CBlockIndex pindexLast;
    int64_t nFirstBlockTime = 1386475638;
    
    pindexLast.nHeight = 101;
    pindexLast.nTime = nFirstBlockTime + 60*20 ; 
    pindexLast.nBits = 0x1f00a000;

    unsigned int out = CalculateLbryNextWorkRequired(&pindexLast, nFirstBlockTime, params);
    arith_uint256 a; 
    a.SetCompact(out);
    unsigned int expected = 0x1f00f000;
    BOOST_CHECK_EQUAL(out,expected);

      
}

// test to see if pow limit is not exceeded
BOOST_AUTO_TEST_CASE(pow_limit_check)
{
    SelectParams(CBaseChainParams::MAIN);
    const Consensus::Params& params = Params().GetConsensus();

    CBlockIndex pindexLast;
    int64_t nFirstBlockTime = 1386475638;
    
    pindexLast.nHeight = 102;
    pindexLast.nTime = nFirstBlockTime + 600 ; //took a long time to generate book, will try to reduce difficulty but it would hit limit  
    pindexLast.nBits = 0x1f00ffff ;

    unsigned int out = CalculateLbryNextWorkRequired(&pindexLast, nFirstBlockTime, params);
    arith_uint256 a; 
    a.SetCompact(out);
    unsigned int expected = 0x1f00ffff;
    BOOST_CHECK_EQUAL(out,expected);

     
}






BOOST_AUTO_TEST_SUITE_END()
