
#include <chainparams.h>
#include <claimtrie.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/locale.hpp>
#include <boost/locale/conversion.hpp>
#include <boost/locale/localization_backend.hpp>
#include <boost/scope_exit.hpp>
#include <boost/scoped_ptr.hpp>

CClaimTrieCacheExpirationFork::CClaimTrieCacheExpirationFork(CClaimTrie* base)
    : CClaimTrieCacheBase(base)
{
    setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight));
}

void CClaimTrieCacheExpirationFork::setExpirationTime(int time)
{
    nExpirationTime = time;
}

int CClaimTrieCacheExpirationFork::expirationTime() const
{
    return nExpirationTime;
}

bool CClaimTrieCacheExpirationFork::incrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    if (CClaimTrieCacheBase::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverHeightUndo)) {
        setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight));
        return true;
    }
    return false;
}

bool CClaimTrieCacheExpirationFork::decrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo)
{
    if (CClaimTrieCacheBase::decrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo)) {
        setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight));
        return true;
    }
    return false;
}

void CClaimTrieCacheExpirationFork::expirationForkActive(int nHeight, bool increment)
{
    if (nHeight == Params().GetConsensus().nExtendedClaimExpirationForkHeight)
        forkForExpirationChange(increment);
}

bool CClaimTrieCacheExpirationFork::forkForExpirationChange(bool increment)
{
    /*
    If increment is True, we have forked to extend the expiration time, thus items in the expiration queue
    will have their expiration extended by "new expiration time - original expiration time"

    If increment is False, we are decremented a block to reverse the fork. Thus items in the expiration queue
    will have their expiration extension removed.
    */

    //look through db for expiration queues, if we haven't already found it in dirty expiration queue
    boost::scoped_ptr<CDBIterator> pcursor(base->db->NewIterator());
    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<uint8_t, int> key;
        if (!pcursor->GetKey(key))
            continue;
        int height = key.second;
        if (key.first == CLAIM_EXP_QUEUE_ROW) {
            expirationQueueRowType row;
            if (pcursor->GetValue(row)) {
                reactivateClaim(row, height, increment);
            } else {
                return error("%s(): error reading expiration queue rows from disk", __func__);
            }
        } else if (key.first == SUPPORT_EXP_QUEUE_ROW) {
            expirationQueueRowType row;
            if (pcursor->GetValue(row)) {
                reactivateSupport(row, height, increment);
            } else {
                return error("%s(): error reading support expiration queue rows from disk", __func__);
            }
        }
    }

    return true;
}

bool CClaimTrieCacheNormalizationFork::shouldNormalize() const
{
    return nNextHeight > Params().GetConsensus().nNormalizedNameForkHeight;
}

std::string CClaimTrieCacheNormalizationFork::normalizeClaimName(const std::string& name, bool force) const
{
    if (!force && !shouldNormalize())
        return name;

    static std::locale utf8;
    static bool initialized = false;
    if (!initialized) {
        static boost::locale::localization_backend_manager manager =
            boost::locale::localization_backend_manager::global();
        manager.select("icu");

        static boost::locale::generator curLocale(manager);
        utf8 = curLocale("en_US.UTF8");
        initialized = true;
    }

    std::string normalized;
    try {
        // Check if it is a valid utf-8 string. If not, it will throw a
        // boost::locale::conv::conversion_error exception which we catch later
        normalized = boost::locale::conv::to_utf<char>(name, "UTF-8", boost::locale::conv::stop);
        if (normalized.empty())
            return name;

        // these methods supposedly only use the "UTF8" portion of the locale object:
        normalized = boost::locale::normalize(normalized, boost::locale::norm_nfd, utf8);
        normalized = boost::locale::fold_case(normalized, utf8);
    } catch (const boost::locale::conv::conversion_error& e) {
        return name;
    } catch (const std::bad_cast& e) {
        LogPrintf("%s() is invalid or dependencies are missing: %s\n", __func__, e.what());
        throw;
    } catch (const std::exception& e) { // TODO: change to use ... with current_exception() in c++11
        LogPrintf("%s() had an unexpected exception: %s\n", __func__, e.what());
        return name;
    }

    return normalized;
}

bool CClaimTrieCacheNormalizationFork::insertClaimIntoTrie(const std::string& name, const CClaimValue& claim, bool fCheckTakeover)
{
    return CClaimTrieCacheExpirationFork::insertClaimIntoTrie(normalizeClaimName(name, overrideInsertNormalization), claim, fCheckTakeover);
}

bool CClaimTrieCacheNormalizationFork::removeClaimFromTrie(const std::string& name, const COutPoint& outPoint, CClaimValue& claim, bool fCheckTakeover)
{
    return CClaimTrieCacheExpirationFork::removeClaimFromTrie(normalizeClaimName(name, overrideRemoveNormalization), outPoint, claim, fCheckTakeover);
}

bool CClaimTrieCacheNormalizationFork::insertSupportIntoMap(const std::string& name, const CSupportValue& support, bool fCheckTakeover)
{
    return CClaimTrieCacheExpirationFork::insertSupportIntoMap(normalizeClaimName(name, overrideInsertNormalization), support, fCheckTakeover);
}

bool CClaimTrieCacheNormalizationFork::removeSupportFromMap(const std::string& name, const COutPoint& outPoint, CSupportValue& support, bool fCheckTakeover)
{
    return CClaimTrieCacheExpirationFork::removeSupportFromMap(normalizeClaimName(name, overrideRemoveNormalization), outPoint, support, fCheckTakeover);
}

bool CClaimTrieCacheNormalizationFork::normalizeAllNamesInTrieIfNecessary(insertUndoType& insertUndo, claimQueueRowType& removeUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    if (nNextHeight != Params().GetConsensus().nNormalizedNameForkHeight)
        return false;

    // run the one-time upgrade of all names that need to change
    // it modifies the (cache) trie as it goes, so we need to grab everything to be modified first

    boost::scoped_ptr<CDBIterator> pcursor(base->db->NewIterator());
    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<uint8_t, std::string> key;
        if (!pcursor->GetKey(key) || key.first != TRIE_NODE_CHILDREN)
            continue;

        const auto& name = key.second;
        const std::string normalized = normalizeClaimName(name, true);
        if (normalized == key.second)
            continue;

        auto supports = getSupportsForName(name);
        for (auto support : supports) {
            // if it's already going to expire just skip it
            if (support.nHeight + expirationTime() <= nNextHeight)
                continue;

            assert(removeSupportFromMap(name, support.outPoint, support, false));
            expireSupportUndo.emplace_back(name, support);
            assert(insertSupportIntoMap(normalized, support, false));
            insertSupportUndo.emplace_back(name, support.outPoint, -1);
        }

        namesToCheckForTakeover.insert(normalized);

        auto cached = cacheData(name, false);
        if (!cached || cached->empty())
            continue;

        auto claimsCopy = cached->claims;
        auto takeoverHeightCopy = cached->nHeightOfLastTakeover;
        for (auto claim : claimsCopy) {
            if (claim.nHeight + expirationTime() <= nNextHeight)
                continue;

            assert(removeClaimFromTrie(name, claim.outPoint, claim, false));
            removeUndo.emplace_back(name, claim);
            assert(insertClaimIntoTrie(normalized, claim, true));
            insertUndo.emplace_back(name, claim.outPoint, -1);
        }

        takeoverHeightUndo.emplace_back(name, takeoverHeightCopy);
    }
    return true;
}

bool CClaimTrieCacheNormalizationFork::incrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    overrideInsertNormalization = normalizeAllNamesInTrieIfNecessary(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverHeightUndo);
    BOOST_SCOPE_EXIT(&overrideInsertNormalization) { overrideInsertNormalization = false; }
    BOOST_SCOPE_EXIT_END
    return CClaimTrieCacheExpirationFork::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverHeightUndo);
}

bool CClaimTrieCacheNormalizationFork::decrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo)
{
    overrideRemoveNormalization = shouldNormalize();
    BOOST_SCOPE_EXIT(&overrideRemoveNormalization) { overrideRemoveNormalization = false; }
    BOOST_SCOPE_EXIT_END
    return CClaimTrieCacheExpirationFork::decrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo);
}

bool CClaimTrieCacheNormalizationFork::getProofForName(const std::string& name, CClaimTrieProof& proof)
{
    return CClaimTrieCacheExpirationFork::getProofForName(normalizeClaimName(name), proof);
}

bool CClaimTrieCacheNormalizationFork::getInfoForName(const std::string& name, CClaimValue& claim) const
{
    return CClaimTrieCacheExpirationFork::getInfoForName(normalizeClaimName(name), claim);
}

CClaimsForNameType CClaimTrieCacheNormalizationFork::getClaimsForName(const std::string& name) const
{
    return CClaimTrieCacheExpirationFork::getClaimsForName(normalizeClaimName(name));
}

int CClaimTrieCacheNormalizationFork::getDelayForName(const std::string& name, const uint160& claimId) const
{
    return CClaimTrieCacheExpirationFork::getDelayForName(normalizeClaimName(name), claimId);
}

std::string CClaimTrieCacheNormalizationFork::adjustNameForValidHeight(const std::string& name, int validHeight) const
{
    return normalizeClaimName(name, validHeight > Params().GetConsensus().nNormalizedNameForkHeight);
}
