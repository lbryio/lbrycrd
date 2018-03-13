#include "claimtrie.h"
#include "main.h"
#include "uint256.h"

#include "test/test_bitcoin.h"
#include <boost/test/unit_test.hpp>
using namespace std;


class CClaimTrieCacheTest : public CClaimTrieCache {
public:
    CClaimTrieCacheTest(CClaimTrie* base):
        CClaimTrieCache(base, false){}

    bool recursiveComputeMerkleHash(CClaimTrieNode* tnCurrent,
                                    std::string sPos) const
    {
        return CClaimTrieCache::recursiveComputeMerkleHash(tnCurrent, sPos);
    }

    bool recursivePruneName(CClaimTrieNode* tnCurrent, unsigned int nPos, std::string sName, bool* pfNullified) const
    {
        return CClaimTrieCache::recursivePruneName(tnCurrent,nPos,sName, pfNullified);
    }
    int cacheSize()
    {
        return cache.size();
    }

    nodeCacheType::iterator getCache(std::string key)
    {
        return cache.find(key);
    }

};

BOOST_FIXTURE_TEST_SUITE(claimtriecache_tests, RegTestingSetup)


BOOST_AUTO_TEST_CASE(merklehash_test)
{
    // check empty trie
    uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CClaimTrieCacheTest cc(pclaimTrie);
    BOOST_CHECK(one == cc.getMerkleHash());

    // check trie with only root node
    CClaimTrieNode base_node;
    cc.recursiveComputeMerkleHash(&base_node, "");
    BOOST_CHECK(one == cc.getMerkleHash());
}

BOOST_AUTO_TEST_CASE(recursiveprune_test)
{
    CClaimTrieCacheTest cc(pclaimTrie);
    BOOST_CHECK_EQUAL(0, cc.cacheSize());

    COutPoint outpoint;
    uint160 claimId;
    CAmount amount(20);
    int height = 0;
    int validAtHeight = 0;
    CClaimValue test_claim(outpoint, claimId, amount, height, validAtHeight);

    CClaimTrieNode base_node;
    // base node has a claim, so it should not be pruned
    base_node.insertClaim(test_claim);

    // node 1 has a claim so it should not be pruned
    CClaimTrieNode node_1;
    const char c = 't';
    base_node.children[c] = &node_1;
    node_1.insertClaim(test_claim);
    // set this just to make sure we get the right CClaimTrieNode back
    node_1.nHeightOfLastTakeover = 10;

    //node 2 does not have a claim so it should be pruned
    // thus we should find pruned node 1 in cache
    CClaimTrieNode node_2;
    const char c_2 = 'e';
    node_1.children[c_2] = &node_2;

    cc.recursivePruneName(&base_node, 0, std::string("te"), NULL);
    BOOST_CHECK_EQUAL(1, cc.cacheSize());
    nodeCacheType::iterator it = cc.getCache(std::string("t"));
    BOOST_CHECK_EQUAL(10, it->second->nHeightOfLastTakeover);
    BOOST_CHECK_EQUAL(1, it->second->claims.size());
    BOOST_CHECK_EQUAL(0, it->second->children.size());
}


BOOST_AUTO_TEST_SUITE_END()
