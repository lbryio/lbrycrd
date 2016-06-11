// Copyright (c) 2016 LBRY, Inc
// Distributed under the BSD 2-Clause License, see the accompanying
// file COPYING or https://opensource.org/licenses/BSD-2-Clause

#include <boost/test/unit_test.hpp>
#include <iostream>
#include "hash.h"
#include "uint256.h"
#include "test/test_bitcoin.h"
#include "chainparams.h"
#include "botan/skein_512.h"
#include "botan/keccak.h"
#include "utilstrencodings.h"

BOOST_FIXTURE_TEST_SUITE(powhash_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(test_skein_hashes)
{
    const int num_hashes = 1;
    const std::string hash_inputs[num_hashes] = {"0000000000000000000000000000000000000000000000000000000000000000"};
    const std::string hash_outputs[num_hashes] = {"ecc5ea6dc8b5eb26f8a98730f11e6a38cf634328325fdfe263a44d45130295ca17168acab2ed32c6449c93986e8a3866ed715504680ca7358eb2ae7ceef0a749"};
    std::vector<unsigned char> vIn;
    vIn.resize(32);
    std::vector<unsigned char> vOut;
    vOut.resize(64);
    std::vector<unsigned char> vExpectedOut;
    vExpectedOut.resize(64);
    for (int i = 0; i < num_hashes; ++i)
    {
        vIn = ParseHex(hash_inputs[i]);
        vExpectedOut = ParseHex(hash_outputs[i]);
        std::reverse(vExpectedOut.begin(), vExpectedOut.end());
        Botan::Skein_512 sk;
        sk.update(&vIn[0], 32);
        sk.final(&vOut[0]);
        BOOST_CHECK(vExpectedOut == vOut);
    }
}

BOOST_AUTO_TEST_CASE(test_keccak_hashes)
{
    const int num_hashes = 1;
    const std::string hash_inputs[num_hashes] = {"0000000000000000000000000000000000000000000000000000000000000000"};
    const std::string hash_outputs[num_hashes] = {"63e5f30e16932f36f608404895bca64bc86f3888a94503d6a8628b54d9ec0d29"};
    std::vector<unsigned char> vIn;
    vIn.resize(32);
    std::vector<unsigned char> vOut;
    vOut.resize(32);
    std::vector<unsigned char> vExpectedOut;
    vExpectedOut.resize(32);
    for (int i = 0; i < num_hashes; ++i)
    {
        vIn = ParseHex(hash_inputs[i]);
        vExpectedOut = ParseHex(hash_outputs[i]);
        std::reverse(vExpectedOut.begin(), vExpectedOut.end());
        Botan::Keccak_1600 kk(256);
        kk.update(&vIn[0], 32);
        kk.final(&vOut[0]);
        BOOST_CHECK(vExpectedOut == vOut);
    }
}

BOOST_AUTO_TEST_SUITE_END()

