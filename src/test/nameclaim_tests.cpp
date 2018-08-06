#include "nameclaim.h"
#include "primitives/transaction.h"
#include <boost/test/unit_test.hpp>
#include "test/test_bitcoin.h"

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

BOOST_AUTO_TEST_SUITE_END()
