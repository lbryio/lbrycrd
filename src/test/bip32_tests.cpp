// Copyright (c) 2013-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "base58.h"
#include "key.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"

#include <string>
#include <vector>

struct TestDerivation {
    std::string pub;
    std::string prv;
    unsigned int nChild;
};

struct TestVector {
    std::string strHexMaster;
    std::vector<TestDerivation> vDerive;

    TestVector(std::string strHexMasterIn) : strHexMaster(strHexMasterIn) {}

    TestVector& operator()(std::string pub, std::string prv, unsigned int nChild) {
        vDerive.push_back(TestDerivation());
        TestDerivation &der = vDerive.back();
        der.pub = pub;
        der.prv = prv;
        der.nChild = nChild;
        return *this;
    }
};

TestVector test1 =
  TestVector("000102030405060708090a0b0c0d0e0f")
    ("Lpub2hkYkGHXktBhLCamCogeqGV7fQatCHyAe3LstjaUdvp6syZNaSGjdxf68TZG1hmc4Dz1dnMnZad1HHiS3DopTy4QxrMcSbiGir5nXrU2xFd",
     "LprvXPsFZUGgrX1X8fNGTeYQawgXAMnj4SeUu4SpYk4vfQ5MtF16HTUVzFEmFmfM4D4JQ66TyH2y3X1SAkMtYvV455cRn2EPLb1DyK21SWCtZac",
     0x80000000)
    ("Lpub2k1xjyazr9JgUoiUQZZHiJbDzE9EaNTBKemmrpxbc81PFFrYzfopm71Vuj7xm1g94SGaW1uDJDh6FggZzFYCXrX9AV9sMucVEnDLqVLA3bY",
     "LprvXS8fZBa9wn8WHGVyfQR3TyndVBM5SX8VafsiWqT3dbGeFXJGhh1b7PbB331gTvSG78zALaMqRsrJtWPcCWmEs9gwqtuXaYi7gKersfwXWn8",
     1)
    ("Lpub2nC5wm8tErBkKFkv8pTAK3Ut3c8LaQBUFyScT6jQYkobUoMgiUE8ifujKoaYAxPV29tdHSMKQpBZqzRPTai6DWM5TgziqYcksWk88PnCeej",
     "LprvXUJnky83LV1a7iYRPfJv4igHYZLBSYrnWzYZ77DraE4rV4oQRVRu4xVQT6Ck4rDJZDjr9PxGWaoj2SuqKrZDeKBPMfsFeJUyM7yEQc2m5F9",
     0x80000002)
    ("Lpub2poMzHxjwj3ABRZ1PQrXg3X2GkzrqwmTbWgHhS2ZidB2teYoXE4Zd7TespmNcppoNfzMtpKvnkkYJKZgAgFdGxVv3axrvVgSbCfGDgJ2VpS",
     "LprvXWv4oVwu3MryytLWeFiHRiiRmiChi6SmrXnEMSX1k6SHtuzXEFGKyQ3L18RN35io4gmEjruBvzddFeeausUQmQd3iHRooEiuxPiT3AByBL9",
     2)
    ("Lpub2s2kpj5h8Ci9Fu2j5g4cZYgLMaHbUY5f2zZaxPZ334jeFkBAazkvpgvaZ9km2cyXqz7Y2posqVeerjugFygpzYyAxNX3kgaGbta7NFjbxNP",
     "LprvXZ9Tdw4rDqXy4MpELWvNKDsjrXVSLgkyJ1fXcQ3V4XzuG1ctJ1xhAyWFgSppVk8E8UtwMn8kqp5niqZ37J1kxoD67XWuMKYd7NBcYeJ6jSm",
     1000000000)
    ("Lpub2tkXJQgwFL6LnEpBCDdLsFgL9oruJ2tgqEx9CfwBF988xsaZ7R1b5o5eByfhst4FKgQ5YAjgNhMFTAJhMqBfqp8Hpuw8ZA9uQrC2cTHvMov",
     "LprvXasE7cg6LxvAahbgT4V6cvsjem4kABa16G45rgRdGcPPy92GpSDMS5fKKJc2QfFJjtJC997hgrcuj1ZQeN2V5krdT2H8UEJLPhjyHjdGM3u",
     0);

TestVector test2 =
  TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("Lpub2hkYkGHXktBhKp66JxkwHUCsubjJsvDzKd8cBgKJcJvyAxaZkb3g4rpgUZeEix8ZxnDoYT1wxf8G15SfAQiybt4MpnZ5H6Cw3DktojFA2uq",
     "LprvXPsFZUGgrX1X8GsbZoch39QHQYw9k4uJaeEYqgokdnCEBE2HTcFSR9QMbqSQEicPe8qtiZVRKd3dGH5zbLskgSwCESJdUvuxYZeEoQkoij6",
     0)
    ("Lpub2m2J1yyVEu1mr5gPSk8pMGcQGZUZatiGsnsyMVLkginvrw2YKrPkDhPw42c8X391B6hyQg1hMjyEAEsM3ZRPwUie6qHkBSMKAhsZEyWPpRJ",
     "LprvXT8zqBxeLXqbeYTthaza6woomWgQT3Pb8oyv1VqCiC4BsCUG2sbWZyycBLiCsnTE3o5icZu9knTkcnip9dNocg6bEcigsR5hfyYoAmjDmEw",
     0xFFFFFFFF)
    ("Lpub2nBMGb11coBt1wtZsC8TpcHxfJhU8NsxxwxWGtFXLTZKKEFzwjp4RmBXfQrFGmk6xcdX4WsJexPppujgpTuWfdBGduuoYAcL369A5mJaAUR",
     "LprvXUJ45nzAiS1hpQg582zDaHVNAFuJzXZHDy4SvtjyMvpaKVhiem1pn3mCnhCBc3Bhx2QcPb6nonxs42TaszywByY9fLx2dvoxTktKw75RfhE",
     1)
    ("Lpub2pzKgbywnSV57ZZhvrH2nZEBzqZyEZoEHPTN7wureLukz48AFFLmKfZuWD3P4eSPYo64u9xYJ5heuhzhjNLTvxeBEC7eki3Xxrp7LFdK9GD",
     "LprvXX72Voy6t5Jtv2MDBh8nYERbVnmp6iUYYQZJmxQJfpB1zKZsxGYXfx9adVPodkPQFymK8tBBDLe5NjEjvtWKCTMSo7UqV2nLwZK4Qns6Jan",
     0xFFFFFFFE)
    ("Lpub2rAMbZvJPqSnQWoJfL6aA4vNua3LHYRjK6BFefcGV7B1SQT6LvGmrBw18sAtG1xqWEjhhf2FLh2WZMA6RFSyeKLdW6S9K18wq5tgsCMpiS8",
     "LprvXYH4QmuTVUGcCyaovAxKuk7nQXFB9h73a7HCJg6iWaSGSftp3wUYCUWgGC8CGAmgoZ41ShxAbiDfBrL7SqQotJ2cd429dEoyH9KaUbZHRAn",
     2)
    ("Lpub2sXPZ18ov1m29mzCPQEG8pJMJdaSXsLoyFo94Sxm7u2bWbnfWZCY2kt1DGFAPbKY2HwvyaXfEQ3W3nDbFM8sxaCD9GGEFNRgWP5sXmFVSUv",
     "LprvXZe6ND7y1eaqxEmheF61tVVkoanHQ228EGu5iTTD9NHrWsEPDaQJP3TgLboCRBrkqrujjTaz3p4MEQDdB9n2wGmLRuXegewgyWf8ZANdd3A",
     0);

void RunTest(const TestVector &test) {
    std::vector<unsigned char> seed = ParseHex(test.strHexMaster);
    CExtKey key;
    CExtPubKey pubkey;
    key.SetMaster(&seed[0], seed.size());
    pubkey = key.Neuter();
    BOOST_FOREACH(const TestDerivation &derive, test.vDerive) {
        unsigned char data[74];
        key.Encode(data);
        pubkey.Encode(data);

        // Test private key
        CBitcoinExtKey b58key; b58key.SetKey(key);
        BOOST_CHECK(b58key.ToString() == derive.prv);

        CBitcoinExtKey b58keyDecodeCheck(derive.prv);
        CExtKey checkKey = b58keyDecodeCheck.GetKey();
        assert(checkKey == key); //ensure a base58 decoded key also matches

        // Test public key
        CBitcoinExtPubKey b58pubkey; b58pubkey.SetKey(pubkey);
        BOOST_CHECK(b58pubkey.ToString() == derive.pub);

        CBitcoinExtPubKey b58PubkeyDecodeCheck(derive.pub);
        CExtPubKey checkPubKey = b58PubkeyDecodeCheck.GetKey();
        assert(checkPubKey == pubkey); //ensure a base58 decoded pubkey also matches

        // Derive new keys
        CExtKey keyNew;
        BOOST_CHECK(key.Derive(keyNew, derive.nChild));
        CExtPubKey pubkeyNew = keyNew.Neuter();
        if (!(derive.nChild & 0x80000000)) {
            // Compare with public derivation
            CExtPubKey pubkeyNew2;
            BOOST_CHECK(pubkey.Derive(pubkeyNew2, derive.nChild));
            BOOST_CHECK(pubkeyNew == pubkeyNew2);
        }
        key = keyNew;
        pubkey = pubkeyNew;

        CDataStream ssPub(SER_DISK, CLIENT_VERSION);
        ssPub << pubkeyNew;
        BOOST_CHECK(ssPub.size() == 75);

        CDataStream ssPriv(SER_DISK, CLIENT_VERSION);
        ssPriv << keyNew;
        BOOST_CHECK(ssPriv.size() == 75);

        CExtPubKey pubCheck;
        CExtKey privCheck;
        ssPub >> pubCheck;
        ssPriv >> privCheck;

        BOOST_CHECK(pubCheck == pubkeyNew);
        BOOST_CHECK(privCheck == keyNew);
    }
}

BOOST_FIXTURE_TEST_SUITE(bip32_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bip32_test1) {
    RunTest(test1);
}

BOOST_AUTO_TEST_CASE(bip32_test2) {
    RunTest(test2);
}

BOOST_AUTO_TEST_SUITE_END()
