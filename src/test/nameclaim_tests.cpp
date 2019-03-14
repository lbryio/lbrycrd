#include <nameclaim.h>
#include <core_io.h>
#include <boost/test/unit_test.hpp>
#include <primitives/transaction.h>
#include <test/test_bitcoin.h>

BOOST_FIXTURE_TEST_SUITE(nameclaim_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(calc_min_claimtrie_fee)
{

    CMutableTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].scriptPubKey = ClaimNameScript("A","test");
    BOOST_CHECK_EQUAL(CalcMinClaimTrieFee(tx, MIN_FEE_PER_NAMECLAIM_CHAR), MIN_FEE_PER_NAMECLAIM_CHAR);

    // check that fee is adjusted based on name length
    CMutableTransaction tx2;
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = ClaimNameScript("ABCDE","test");
    BOOST_CHECK_EQUAL(CalcMinClaimTrieFee(tx2,MIN_FEE_PER_NAMECLAIM_CHAR), MIN_FEE_PER_NAMECLAIM_CHAR*5);

    // check that multiple OP_CLAIM_NAME outputs are counted
    CMutableTransaction tx3;
    tx3.vout.resize(2);
    tx3.vout[0].scriptPubKey = ClaimNameScript("A","test");
    tx3.vout[1].scriptPubKey = ClaimNameScript("AB","test");
    BOOST_CHECK_EQUAL(CalcMinClaimTrieFee(tx3,MIN_FEE_PER_NAMECLAIM_CHAR), MIN_FEE_PER_NAMECLAIM_CHAR*3);

    // if tx has no claim minimum fee is 0
    CMutableTransaction tx4;
    tx4.vout.resize(1);
    BOOST_CHECK_EQUAL(CalcMinClaimTrieFee(tx4,MIN_FEE_PER_NAMECLAIM_CHAR), 0);

}

BOOST_AUTO_TEST_CASE(scriptToAsmStr_output)
{
    auto claimScript = CScript() << OP_CLAIM_NAME
                                 << std::vector<unsigned char>{ 'h', 'i' }
                                 << std::vector<unsigned char>{ 'd', 'a', 't', 'a' } << OP_2DROP << OP_DROP << OP_TRUE;

    auto result = ScriptToAsmStr(claimScript, false);
    BOOST_CHECK(result.find("6869 64617461") != std::string::npos);

    auto updateScript = CScript() << OP_UPDATE_CLAIM
                                  << std::vector<unsigned char>{ 'h', 'i' }
                                  << std::vector<unsigned char>{ 'i', 'd' }
                                  << std::vector<unsigned char>{ 'd', 'a', 't', 'a' } << OP_2DROP << OP_2DROP << OP_TRUE;

    result = ScriptToAsmStr(updateScript, false);
    BOOST_CHECK(result.find("64617461") != std::string::npos);

    auto supportScript = CScript() << OP_SUPPORT_CLAIM
                                 << std::vector<unsigned char>{ 'h', 'i' }
                                 << std::vector<unsigned char>{ 'i', 'd' } << OP_2DROP << OP_DROP << OP_TRUE;

    result = ScriptToAsmStr(supportScript, false);
    BOOST_CHECK(result.find("6869 6964") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
