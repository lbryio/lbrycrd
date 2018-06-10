#include "claimtrie.h"
#include "main.h"
#include "uint256.h"

#include "test/test_bitcoin.h"
#include <boost/test/unit_test.hpp>
using namespace std;

#include "claimtriedb.cpp" /// avoid linker problems

struct claimtriedb_test_helper
{
    static void migrate(CClaimTrieDb &db)
    {
        db.doMigrate();
    }
};

BOOST_FIXTURE_TEST_SUITE(claimtriedb_tests, BasicTestingSetup)


BOOST_AUTO_TEST_CASE(hash_test)
{
    const size_t intclaimQueueRowType = hashType<int, claimQueueRowType>();
    const size_t intsupportQueueRowType = hashType<int, supportQueueRowType>();
    const size_t stringCClaimTrieNode = hashType<std::string, CClaimTrieNode>();
    const size_t stringqueueNameRowType = hashType<std::string, queueNameRowType>();
    const size_t intexpirationQueueRowType = hashType<int, expirationQueueRowType>();
    const size_t uint160CClaimIndexElement = hashType<uint160, CClaimIndexElement>();
    const size_t stringsupportMapEntryType = hashType<std::string, supportMapEntryType>();
    const size_t stringsupportQueueNameRowType = hashType<std::string, supportQueueNameRowType>();
    const size_t intsupportExpirationQueueRowType = hashType<int, supportExpirationQueueRowType>();

    BOOST_CHECK(intclaimQueueRowType                == 2291382435);
    BOOST_CHECK(stringCClaimTrieNode                == 2689964764);
    BOOST_CHECK(intsupportQueueRowType              == 2519094245);
    BOOST_CHECK(stringqueueNameRowType              == 2566197209);
    BOOST_CHECK(stringsupportMapEntryType           == 212036296);
    BOOST_CHECK(intexpirationQueueRowType           == 2977646682);
    BOOST_CHECK(uint160CClaimIndexElement           == 1885673905);
    BOOST_CHECK(stringsupportQueueNameRowType       == 389061475);
    BOOST_CHECK(intsupportExpirationQueueRowType    == 3108630299);
}

BOOST_AUTO_TEST_CASE(migration_test)
{
    const int value = 20;
    const std::string test("test");

    CClaimTrieNode base_node;
    const CClaimValue test_claim(COutPoint(), uint160(), CAmount(value), 0, 0);
    const CClaimIndexElement test_element = { test, test_claim };
    const CSupportValue test_support(COutPoint(), uint160(), CAmount(value), 0, 0);

    base_node.claims.push_back(test_claim);
    const supportMapEntryType supportMap(1, test_support);
    const queueNameRowType nameRows(1, outPointHeightType(COutPoint(), value));
    const claimQueueRowType claimRows(1, claimQueueEntryType(test, test_claim));
    const supportQueueRowType supportRows(1, supportQueueEntryType(test, test_support));
    const expirationQueueRowType expirationRows(1, nameOutPointType(test, COutPoint()));
    const supportQueueNameRowType supportNameRows(1, outPointHeightType(COutPoint(), value));
    const supportExpirationQueueRowType supportExpirationRows(1, nameOutPointType(test, COutPoint()));

    CClaimTrieDb db(true, false);

    /// fill out old char keys
    db.Write(std::make_pair('n', test), base_node);
    db.Write(std::make_pair('i', uint160()), test_element);
    db.Write(std::make_pair('r', value), claimRows);
    db.Write(std::make_pair('m', test), nameRows);
    db.Write(std::make_pair('e', value), expirationRows);
    db.Write(std::make_pair('s', test), supportMap);
    db.Write(std::make_pair('u', value), supportRows);
    db.Write(std::make_pair('p', test), supportNameRows);
    db.Write(std::make_pair('x', value), supportExpirationRows);
    db.Sync();

    /// migrations is done here
    claimtriedb_test_helper::migrate(db);

    CClaimTrieNode node;
    db.getQueueRow(test, node);
    BOOST_CHECK(node == base_node);

    CClaimIndexElement element;
    db.getQueueRow(uint160(), element);
    BOOST_CHECK(element.name == test_element.name);
    BOOST_CHECK(element.claim == test_element.claim);

    claimQueueRowType claims;
    db.getQueueRow(value, claims);
    BOOST_CHECK(claims.size() == 1);
    BOOST_CHECK(claims[0].first == claimRows[0].first);
    BOOST_CHECK(claims[0].second == claimRows[0].second);

    queueNameRowType names;
    db.getQueueRow(test, names);
    BOOST_CHECK(names.size() == 1);
    BOOST_CHECK(names[0].nHeight == nameRows[0].nHeight);

    expirationQueueRowType exps;
    db.getQueueRow(value, exps);
    BOOST_CHECK(exps.size() == 1);
    BOOST_CHECK(exps[0].name == expirationRows[0].name);

    supportMapEntryType sups;
    db.getQueueRow(test, sups);
    BOOST_CHECK(sups.size() == 1);
    BOOST_CHECK(sups[0] == supportMap[0]);

    supportQueueRowType supRows;
    db.getQueueRow(value, supRows);
    BOOST_CHECK(supRows.size() == 1);
    BOOST_CHECK(supRows[0] == supportRows[0]);

    supportQueueNameRowType supNames;
    db.getQueueRow(test, supNames);
    BOOST_CHECK(supNames.size() == 1);
    BOOST_CHECK(supNames[0].nHeight == supportNameRows[0].nHeight);

    supportExpirationQueueRowType supExps;
    db.getQueueRow(value, supExps);
    BOOST_CHECK(supExps.size() == 1);
    BOOST_CHECK(supExps[0].name == supportExpirationRows[0].name);
}

BOOST_AUTO_TEST_SUITE_END()
