#include <claimtrie.h>
#include <prefixtrie.h>
#include <random.h>

#include <boost/test/unit_test.hpp>
#include <test/claimtriefixture.h>
#include <test/test_bitcoin.h>

#include <chrono>

std::vector<std::string> random_strings(std::size_t count)
{
    static auto& chrs = "0123456789"
                        "abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    FastRandomContext frc(true);

    std::unordered_set<std::string> strings;
    strings.reserve(count);

    while(strings.size() < count) {
        auto length = frc.randrange(sizeof(chrs) - 2) + 1;

        std::string s;
        s.reserve(length);

        while (length--)
            s += chrs[frc.randrange(sizeof(chrs) - 1)];

        strings.emplace(s);
    }

    std::vector<std::string> ret(strings.begin(), strings.end());
    std::random_shuffle(ret.begin(), ret.end(), [&frc](std::size_t n) { return frc.randrange(n); });
    return ret;
}

using namespace std;

BOOST_FIXTURE_TEST_SUITE(prefixtrie_tests, RegTestingSetup)

#ifndef MAC_OSX // can't find a random number generator that produces the same sequence on OSX
BOOST_AUTO_TEST_CASE(triehash_fuzzer_test)
{
    ClaimTrieChainFixture fixture;

    auto envClaims = std::getenv("TRIEHASH_FUZZER_CLAIMS");
    auto envBlocks = std::getenv("TRIEHASH_FUZZER_BLOCKS");

    const int claimsPerBlock = envClaims ? std::atoi(envClaims) : 100;
    const int blocks = envBlocks ? std::atoi(envBlocks) : 13;

    auto names = random_strings(blocks * claimsPerBlock);

    FastRandomContext frc(true);
    std::unordered_map<std::string, std::vector<CMutableTransaction>> existingClaims;
    std::vector<CMutableTransaction> existingSupports;
    std::string value(1024, 'c');

    std::vector<CTransaction> cb {fixture.GetCoinbase()};
    for (int i = 0; i < blocks; ++i) {
        for (int j = 0; j < claimsPerBlock; ++j) {
            auto name = names[i * claimsPerBlock + j];
            auto supportFront = frc.randrange(4) == 0;
            auto supportBack = frc.randrange(4) == 0;
            auto removeClaim = frc.randrange(4) == 0;
            auto removeSupport = frc.randrange(4) == 0;
            auto hit = existingClaims.find(name);
            if (supportFront && hit != existingClaims.end() && hit->second.size()) {
                auto tx = fixture.MakeSupport(cb.back(), hit->second[frc.rand64() % hit->second.size()], name, 2);
                existingSupports.push_back(tx);
                cb.emplace_back(std::move(tx));
            }
            if (removeClaim && hit != existingClaims.end() && hit->second.size()) {
                auto idx = frc.rand64() % hit->second.size();
                fixture.Spend(hit->second[idx]);
                hit->second.erase(hit->second.begin() + idx);
            } else {
                auto tx = fixture.MakeClaim(cb.back(), name, value, 2);
                existingClaims[name].push_back(tx);
                hit = existingClaims.find(name);
                cb.emplace_back(std::move(tx));
            }
            if (supportBack && hit != existingClaims.end() && hit->second.size()) {
                auto tx = fixture.MakeSupport(cb.back(), hit->second[frc.rand64() % hit->second.size()], name, 2);
                existingSupports.push_back(tx);
                cb.emplace_back(std::move(tx));
            }
            if (removeSupport && (i & 7) == 7 && !existingSupports.empty()) {
                const auto tidx = frc.rand64() % existingSupports.size();
                const auto tx = existingSupports[tidx];
                fixture.Spend(tx);
                existingSupports.erase(existingSupports.begin() + tidx);
            }
            if (cb.back().GetValueOut() < 10 || cb.size() > 40000) {
                cb.clear();
                cb.push_back(fixture.GetCoinbase());
            }
        }
        fixture.IncrementBlocks(1);
        if (blocks > 13 && i % 50 == 0) // travisCI needs some periodic output
            std::cerr << "In triehash_fuzzer_test with " << fixture.getTotalNamesInTrie() << " names at block " << i << std::endl;
    }

    if (blocks == 1000 && claimsPerBlock == 100)
        BOOST_CHECK_EQUAL(fixture.getMerkleHash().GetHex(), "28825257a129eef69cab87d6255c8359fc6dc083ca7f09222526e3a7971f382d");
    else if (blocks == 13 && claimsPerBlock == 100)
        BOOST_CHECK_EQUAL(fixture.getMerkleHash().GetHex(), "4e5984d6984f5f05d50e821e6228d56bcfbd16ca2093cd0308f6ff1c2bc8689a");
    else
        std::cerr << "Hash: "  << fixture.getMerkleHash().GetHex() << std::endl;
}
#endif

BOOST_AUTO_TEST_CASE(insert_erase_test)
{
    CPrefixTrie<std::string, CClaimTrieData> root;

    BOOST_CHECK(root.empty());
    BOOST_CHECK_EQUAL(root.height(), 0);

    CClaimTrieData data;
    data.insertClaim(CClaimValue{});
    BOOST_CHECK(root.insert("abc", std::move(data)) != root.end());
    BOOST_CHECK_EQUAL(root.height(), 1);

    data.insertClaim(CClaimValue{});
    BOOST_CHECK(root.insert("abd", std::move(data)) != root.end());
    BOOST_CHECK_EQUAL(root.height(), 3);

    // test expanding on ab
    BOOST_CHECK(root.find("ab") != root.end());

    data.insertClaim(CClaimValue{});
    BOOST_CHECK(root.insert("a", std::move(data)) != root.end());
    BOOST_CHECK_EQUAL(root.height(), 4);

    // test ab again
    BOOST_CHECK(root.find("ab") != root.end());

    BOOST_CHECK(root.erase("abd"));
    BOOST_CHECK(root.find("abd") == root.end());
    BOOST_CHECK_EQUAL(root.height(), 2);

    // collapsing of ab-c to abc so ab does not present any more
    BOOST_CHECK(root.find("ab") == root.end());

    // a has children so only data is erased
    BOOST_CHECK(root.erase("a"));
    BOOST_CHECK(root.find("a") == root.end());

    // erasing abc will erase all node refers to a
    BOOST_CHECK(root.erase("abc"));
    BOOST_CHECK(root.empty());
    BOOST_CHECK_EQUAL(root.height(), 0);
    BOOST_CHECK(root.find("a") == root.end());
    BOOST_CHECK(root.find("ab") == root.end());
    BOOST_CHECK(root.find("abc") == root.end());
    BOOST_CHECK(root.find("abd") == root.end());
}

BOOST_AUTO_TEST_CASE(iterate_test)
{
    CPrefixTrie<std::string, CClaimTrieData> root;

    BOOST_CHECK(root.empty());
    BOOST_CHECK_EQUAL(root.height(), 0);

    BOOST_CHECK(root.insert("abc", CClaimTrieData{}) != root.end());
    BOOST_CHECK_EQUAL(root.height(), 1);

    BOOST_CHECK(root.insert("abd", CClaimTrieData{}) != root.end());
    BOOST_CHECK_EQUAL(root.height(), 3);

    BOOST_CHECK(root.insert("tdb", CClaimTrieData{}) != root.end());
    BOOST_CHECK_EQUAL(root.height(), 4);

    for (auto it = root.begin(); it != root.end(); ++it) {
        auto& key = it.key();
        if (key.empty() ||
            key == "ab" ||
            key == "abc" ||
            key == "abd" ||
            key == "tdb") {
        } else {
            BOOST_CHECK(false); // should not happen
        }
    }

    auto nodes = root.nodes("abd");
    BOOST_CHECK_EQUAL(nodes.size(), 3);

    BOOST_CHECK_EQUAL(nodes[0].key(), "");
    BOOST_CHECK(nodes[0].hasChildren());
    BOOST_CHECK_EQUAL(nodes[0].children().size(), 2);

    // children of ""
    for (auto& child : nodes[0].children()) {
        auto& key = child.key();
        if (key == "ab" || key == "tdb") {
        } else {
            BOOST_CHECK(false); // should not happen
        }
    }

    BOOST_CHECK_EQUAL(nodes[1].key(), "ab");
    BOOST_CHECK(nodes[1].hasChildren());
    BOOST_CHECK_EQUAL(nodes[1].children().size(), 2);

    // children of "ab"
    for (auto& child : nodes[1].children()) {
        auto& key = child.key();
        if (key == "abc" || key == "abd") {
        } else {
            BOOST_CHECK(false); // should not happen
        }
    }

    BOOST_CHECK_EQUAL(nodes[2].key(), "abd");
    BOOST_CHECK(!nodes[2].hasChildren());
    BOOST_CHECK_EQUAL(nodes[2].children().size(), 0);
}

BOOST_AUTO_TEST_CASE(erase_cleanup_test)
{
    CPrefixTrie<std::string, CClaimTrieData> root;

    CClaimTrieData data;
    data.insertClaim(CClaimValue{});
    BOOST_CHECK(root.insert("a", std::move(data)) != root.end());
    data.insertClaim(CClaimValue{});
    BOOST_CHECK(root.insert("adata", std::move(data)) != root.end());
    data.insertClaim(CClaimValue{});
    BOOST_CHECK(root.insert("adata2", std::move(data)) != root.end());
    data.insertClaim(CClaimValue{});
    BOOST_CHECK(root.insert("adota", std::move(data)) != root.end());
    BOOST_CHECK_EQUAL(root.height(), 5);
    BOOST_CHECK(root.erase("adata"));
    BOOST_CHECK(root.erase("adata2"));
    BOOST_CHECK_EQUAL(root.height(), 2);
}

BOOST_AUTO_TEST_CASE(add_many_nodes) {
    // this if for testing performance and making sure erasure goes all the way to zero
    CPrefixTrie<std::string, CClaimTrieData> trie;
    std::size_t count = 100000; // bump this up to 1M for a better test
    auto strings = random_strings(count);

    auto s1 = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < count; i++) {
        CClaimTrieData d; d.nHeightOfLastTakeover = i;
        trie.insert(strings[i], std::move(d));
    }

    auto s2 = std::chrono::high_resolution_clock::now();

    auto nodes = trie.height();
    // run with --log_level=message to see these:
    BOOST_TEST_MESSAGE("Nodes:" + std::to_string(nodes));
    BOOST_TEST_MESSAGE("Insertion sec: " + std::to_string(std::chrono::duration_cast<std::chrono::duration<float> >(s2 - s1).count()));

    auto s3 = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < count; i++) {
        auto& value = trie.at(strings[i]);
        BOOST_CHECK_EQUAL(value.nHeightOfLastTakeover, i);
    }

    auto s4 = std::chrono::high_resolution_clock::now();
    BOOST_TEST_MESSAGE("Lookup sec: " + std::to_string(std::chrono::duration_cast<std::chrono::duration<float> >(s4 - s3).count()));

    auto s5 = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < count; i++) {
        auto value = trie.nodes(strings[i]);
        BOOST_CHECK(!value.empty());
    }

    auto s6 = std::chrono::high_resolution_clock::now();
    BOOST_TEST_MESSAGE("Parents sec: " + std::to_string(std::chrono::duration_cast<std::chrono::duration<float> >(s6 - s5).count()));

    auto s7 = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < count; i++) {
        trie.erase(strings[i]);
    }

    BOOST_CHECK_EQUAL(trie.height(), 0);

    auto s8 = std::chrono::high_resolution_clock::now();
    BOOST_TEST_MESSAGE("Deletion sec: " + std::to_string(std::chrono::duration_cast<std::chrono::duration<float> >(s8 - s7).count()));
}

BOOST_AUTO_TEST_SUITE_END()
