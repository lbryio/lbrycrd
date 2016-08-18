#include "arith_uint256.h"
#include "chainparams.h"
#include "lbry.h"
#include "main.h"
#include "test/test_bitcoin.h"
#include "hash.h" 
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


boost::test_tools::predicate_result
check_powhash_equals(std::string test_string,std::string expected_hash_hex)
{
    std::vector<unsigned char> test_vect(test_string.begin(),test_string.end());
    uint256 expected;
    expected.SetHex(expected_hash_hex);
    uint256 hash = PoWHash(test_vect);

    //std::cout<<"hash :"<<hash.ToString()<<"\n";

    if (hash == expected){
        return true;
    }
    else
    {
        boost::test_tools::predicate_result res( false );
        res.message() << "Hash of " << test_string << " != " << expected_hash_hex;
        return res;
    }
}

BOOST_AUTO_TEST_CASE(lbry_pow_test)
{
    
    BOOST_CHECK(check_powhash_equals("test string","485f3920d48a0448034b0852d1489cfa475341176838c7d36896765221be35ce"));
    BOOST_CHECK(check_powhash_equals(std::string(70,'a'),"eb44af2f41e7c6522fb8be4773661be5baa430b8b2c3a670247e9ab060608b75")); 
    BOOST_CHECK(check_powhash_equals(std::string(140,'d'),"74044747b7c1ff867eb09a84d026b02d8dc539fb6adcec3536f3dfa9266495d9"));

}




BOOST_AUTO_TEST_SUITE_END()
