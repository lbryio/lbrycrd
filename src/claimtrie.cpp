#include "claimtrie.h"
#include "coins.h"
#include "hash.h"

#include <iostream>
#include <algorithm>

#define HASH_BLOCK 'h'
#define CURRENT_HEIGHT 't'

#include "claimtriedb.cpp" /// avoid linker problems

std::vector<unsigned char> heightToVch(int n)
{
    std::vector<unsigned char> vchHeight;
    vchHeight.resize(8);
    vchHeight[0] = 0;
    vchHeight[1] = 0;
    vchHeight[2] = 0;
    vchHeight[3] = 0;
    vchHeight[4] = n >> 24;
    vchHeight[5] = n >> 16;
    vchHeight[6] = n >> 8;
    vchHeight[7] = n;
    return vchHeight;
}

uint256 getValueHash(COutPoint outPoint, int nHeightOfLastTakeover)
{
    CHash256 txHasher;
    txHasher.Write(outPoint.hash.begin(), outPoint.hash.size());
    std::vector<unsigned char> vchtxHash(txHasher.OUTPUT_SIZE);
    txHasher.Finalize(&(vchtxHash[0]));

    CHash256 nOutHasher;
    std::stringstream ss;
    ss << outPoint.n;
    std::string snOut = ss.str();
    nOutHasher.Write((unsigned char*) snOut.data(), snOut.size());
    std::vector<unsigned char> vchnOutHash(nOutHasher.OUTPUT_SIZE);
    nOutHasher.Finalize(&(vchnOutHash[0]));

    CHash256 takeoverHasher;
    std::vector<unsigned char> vchTakeoverHeightToHash = heightToVch(nHeightOfLastTakeover);
    takeoverHasher.Write(vchTakeoverHeightToHash.data(), vchTakeoverHeightToHash.size());
    std::vector<unsigned char> vchTakeoverHash(takeoverHasher.OUTPUT_SIZE);
    takeoverHasher.Finalize(&(vchTakeoverHash[0]));

    CHash256 hasher;
    hasher.Write(vchtxHash.data(), vchtxHash.size());
    hasher.Write(vchnOutHash.data(), vchnOutHash.size());
    hasher.Write(vchTakeoverHash.data(), vchTakeoverHash.size());
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Finalize(&(vchHash[0]));

    uint256 valueHash(vchHash);
    return valueHash;
}

bool CClaimTrieNode::insertClaim(CClaimValue claim)
{
    LogPrintf("%s: Inserting %s:%d (amount: %d)  into the claim trie\n", __func__, claim.outPoint.hash.ToString(), claim.outPoint.n, claim.nAmount);
    claims.push_back(claim);
    return true;
}

bool CClaimTrieNode::removeClaim(const COutPoint& outPoint, CClaimValue& claim)
{
    LogPrintf("%s: Removing txid: %s, nOut: %d from the claim trie\n", __func__, outPoint.hash.ToString(), outPoint.n);

    std::vector<CClaimValue>::iterator itClaims;
    for (itClaims = claims.begin(); itClaims != claims.end(); ++itClaims) {
        if (itClaims->outPoint == outPoint) {
            std::swap(claim, *itClaims);
            break;
        }
    }
    if (itClaims != claims.end()) {
        claims.erase(itClaims);
    } else {
        LogPrintf("CClaimTrieNode::%s() : asked to remove a claim that doesn't exist\n", __func__);
        LogPrintf("CClaimTrieNode::%s() : claims that do exist:\n", __func__);
        for (unsigned int i = 0; i < claims.size(); i++) {
            LogPrintf("\ttxhash: %s, nOut: %d:\n", claims[i].outPoint.hash.ToString(), claims[i].outPoint.n);
        }
        return false;
    }
    return true;
}

bool CClaimTrieNode::getBestClaim(CClaimValue& claim) const
{
    if (claims.empty()) {
        return false;
    } else {
        claim = claims.front();
        return true;
    }
}

bool CClaimTrieNode::haveClaim(const COutPoint& outPoint) const
{
    for (std::vector<CClaimValue>::const_iterator itclaim = claims.begin(); itclaim != claims.end(); ++itclaim) {
        if (itclaim->outPoint == outPoint) return true;
    }
    return false;
}

void CClaimTrieNode::reorderClaims(supportMapEntryType& supports)
{
    std::vector<CClaimValue>::iterator itclaim;

    for (itclaim = claims.begin(); itclaim != claims.end(); ++itclaim) {
        itclaim->nEffectiveAmount = itclaim->nAmount;
    }

    for (supportMapEntryType::iterator itsupport = supports.begin(); itsupport != supports.end(); ++itsupport) {
        for (itclaim = claims.begin(); itclaim != claims.end(); ++itclaim) {
            if (itsupport->supportedClaimId == itclaim->claimId) {
                itclaim->nEffectiveAmount += itsupport->nAmount;
                break;
            }
        }
    }

    std::make_heap(claims.begin(), claims.end());
}

bool CClaimTrieNode::empty() const
{
    return children.empty() && claims.empty();
}

CClaimTrie::CClaimTrie(bool fMemory, bool fWipe, int nProportionalDelayFactor)
                    : nCurrentHeight(0)
                    , nExpirationTime(Params().GetConsensus().nOriginalClaimExpirationTime)
                    , nProportionalDelayFactor(nProportionalDelayFactor)
                    , db(fMemory, fWipe)
                    , root(one)
{
}

CClaimTrie::~CClaimTrie()
{
}

uint256 CClaimTrie::getMerkleHash()
{
    return root.hash;
}

bool CClaimTrie::empty() const
{
    return root.empty();
}

bool CClaimTrie::queueEmpty() const
{
    return db.keyTypeEmpty<int, claimQueueRowType>();
}

bool CClaimTrie::expirationQueueEmpty() const
{
    return db.keyTypeEmpty<int, expirationQueueRowType>();
}

bool CClaimTrie::supportEmpty() const
{
    return db.keyTypeEmpty<std::string, supportMapEntryType>();
}

bool CClaimTrie::supportQueueEmpty() const
{
    return db.keyTypeEmpty<int, supportQueueRowType>();
}

void CClaimTrie::setExpirationTime(int t)
{
    nExpirationTime = t;
    LogPrintf("%s: Expiration time is now %d\n", __func__, nExpirationTime);
}

void CClaimTrie::clear()
{
    clear(&root);
}

void CClaimTrie::clear(CClaimTrieNode* current)
{
    for (nodeMapType::const_iterator itchildren = current->children.begin(); itchildren != current->children.end(); ++itchildren) {
        clear(itchildren->second);
        delete itchildren->second;
    }
}

bool CClaimTrie::haveClaim(const std::string& name, const COutPoint& outPoint) const
{
    const CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname) {
        nodeMapType::const_iterator itchildren = current->children.find(*itname);
        if (itchildren == current->children.end()) return false;
        current = itchildren->second;
    }
    return current->haveClaim(outPoint);
}

bool CClaimTrie::haveSupport(const std::string& name, const COutPoint& outPoint) const
{
    supportMapEntryType node;
    if (!db.getQueueRow(name, node)) return false;
    for (supportMapEntryType::const_iterator itnode = node.begin(); itnode != node.end(); ++itnode) {
        if (itnode->outPoint == outPoint) return true;
    }
    return false;
}

bool CClaimTrie::haveClaimInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    queueNameRowType nameRow;
    if (!db.getQueueRow(name, nameRow)) return false;
    queueNameRowType::const_iterator itNameRow;
    for (itNameRow = nameRow.begin(); itNameRow != nameRow.end(); ++itNameRow) {
        if (itNameRow->outPoint == outPoint) {
            nValidAtHeight = itNameRow->nHeight;
            break;
        }
    }
    if (itNameRow == nameRow.end()) return false;
    claimQueueRowType row;
    if (db.getQueueRow(nValidAtHeight, row)) {
        for (claimQueueRowType::const_iterator itRow = row.begin(); itRow != row.end(); ++itRow) {
            if (itRow->first == name && itRow->second.outPoint == outPoint) {
                if (itRow->second.nValidAtHeight != nValidAtHeight) {
                    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, itRow->second.nValidAtHeight, nCurrentHeight);
                }
                return true;
            }
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, nCurrentHeight);
    return false;
}

bool CClaimTrie::haveSupportInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    supportQueueNameRowType nameRow;
    if (!db.getQueueRow(name, nameRow)) return false;
    supportQueueNameRowType::const_iterator itNameRow;
    for (itNameRow = nameRow.begin(); itNameRow != nameRow.end(); ++itNameRow) {
        if (itNameRow->outPoint == outPoint) {
            nValidAtHeight = itNameRow->nHeight;
            break;
        }
    }
    if (itNameRow == nameRow.end()) return false;
    supportQueueRowType row;
    if (db.getQueueRow(nValidAtHeight, row)) {
        for (supportQueueRowType::const_iterator itRow = row.begin(); itRow != row.end(); ++itRow) {
            if (itRow->first == name && itRow->second.outPoint == outPoint) {
                if (itRow->second.nValidAtHeight != nValidAtHeight) {
                    LogPrintf("%s: An inconsistency was found in the support queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, itRow->second.nValidAtHeight, nCurrentHeight);
                }
                return true;
            }
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, nCurrentHeight);
    return false;
}

unsigned int CClaimTrie::getTotalNamesInTrie() const
{
    if (empty()) {
        return 0;
    }
    const CClaimTrieNode* current = &root;
    return getTotalNamesRecursive(current);
}

unsigned int CClaimTrie::getTotalNamesRecursive(const CClaimTrieNode* current) const
{
    unsigned int names_in_subtrie = 0;
    if (!(current->claims.empty())) {
        names_in_subtrie += 1;
    }
    for (nodeMapType::const_iterator it = current->children.begin(); it != current->children.end(); ++it) {
        names_in_subtrie += getTotalNamesRecursive(it->second);
    }
    return names_in_subtrie;
}

unsigned int CClaimTrie::getTotalClaimsInTrie() const
{
    if (empty()) {
        return 0;
    }
    const CClaimTrieNode* current = &root;
    return getTotalClaimsRecursive(current);
}

unsigned int CClaimTrie::getTotalClaimsRecursive(const CClaimTrieNode* current) const
{
    unsigned int claims_in_subtrie = current->claims.size();
    for (nodeMapType::const_iterator it = current->children.begin(); it != current->children.end(); ++it) {
        claims_in_subtrie += getTotalClaimsRecursive(it->second);
    }
    return claims_in_subtrie;
}

CAmount CClaimTrie::getTotalValueOfClaimsInTrie(bool fControllingOnly) const
{
    if (empty()) {
        return 0;
    }
    const CClaimTrieNode* current = &root;
    return getTotalValueOfClaimsRecursive(current, fControllingOnly);
}

CAmount CClaimTrie::getTotalValueOfClaimsRecursive(const CClaimTrieNode* current, bool fControllingOnly) const
{
    CAmount value_in_subtrie = 0;
    for (std::vector<CClaimValue>::const_iterator itclaim = current->claims.begin(); itclaim != current->claims.end(); ++itclaim) {
        value_in_subtrie += itclaim->nAmount;
        if (fControllingOnly) {
            break;
        }
    }
    for (nodeMapType::const_iterator itchild = current->children.begin(); itchild != current->children.end(); ++itchild) {
        value_in_subtrie += getTotalValueOfClaimsRecursive(itchild->second, fControllingOnly);
    }
    return value_in_subtrie;
}

bool CClaimTrie::recursiveFlattenTrie(const std::string& name, const CClaimTrieNode* current, std::vector<namedNodeType>& nodes) const
{
    namedNodeType node(name, *current);
    nodes.push_back(node);
    for (nodeMapType::const_iterator it = current->children.begin(); it != current->children.end(); ++it) {
        std::stringstream ss;
        ss << name << it->first;
        if (!recursiveFlattenTrie(ss.str(), it->second, nodes)) return false;
    }
    return true;
}

std::vector<namedNodeType> CClaimTrie::flattenTrie() const
{
    std::vector<namedNodeType> nodes;
    if (!recursiveFlattenTrie("", &root, nodes)) {
        LogPrintf("%s: Something went wrong flattening the trie", __func__);
    }
    return nodes;
}

const CClaimTrieNode* CClaimTrie::getNodeForName(const std::string& name) const
{
    const CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname) {
        nodeMapType::const_iterator itchildren = current->children.find(*itname);
        if (itchildren == current->children.end()) return NULL;
        current = itchildren->second;
    }
    return current;
}

bool CClaimTrie::getInfoForName(const std::string& name, CClaimValue& claim) const
{
    const CClaimTrieNode* current = getNodeForName(name);
    if (current) {
        return current->getBestClaim(claim);
    } else {
        return false;
    }
}

bool CClaimTrie::getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const
{
    const CClaimTrieNode* current = getNodeForName(name);
    if (current && !current->claims.empty()) {
        lastTakeoverHeight = current->nHeightOfLastTakeover;
        return true;
    } else {
        return false;
    }
}

claimsForNameType CClaimTrie::getClaimsForName(const std::string& name) const
{
    std::vector<CClaimValue> claims;
    std::vector<CSupportValue> supports;
    int nLastTakeoverHeight = 0;
    const CClaimTrieNode* current = getNodeForName(name);
    if (current) {
        if (!current->claims.empty()) {
            nLastTakeoverHeight = current->nHeightOfLastTakeover;
        }
        for (std::vector<CClaimValue>::const_iterator itClaims = current->claims.begin(); itClaims != current->claims.end(); ++itClaims) {
            claims.push_back(*itClaims);
        }
    }
    supportMapEntryType supportNode;
    if (db.getQueueRow(name, supportNode)) {
        for (std::vector<CSupportValue>::const_iterator itSupports = supportNode.begin(); itSupports != supportNode.end(); ++itSupports) {
            supports.push_back(*itSupports);
        }
    }
    queueNameRowType namedClaimRow;
    if (db.getQueueRow(name, namedClaimRow)) {
        for (queueNameRowType::const_iterator itClaimsForName = namedClaimRow.begin(); itClaimsForName != namedClaimRow.end(); ++itClaimsForName) {
            claimQueueRowType claimRow;
            if (db.getQueueRow(itClaimsForName->nHeight, claimRow)) {
                for (claimQueueRowType::const_iterator itClaimRow = claimRow.begin(); itClaimRow != claimRow.end(); ++itClaimRow) {
                     if (itClaimRow->first == name && itClaimRow->second.outPoint == itClaimsForName->outPoint) {
                         claims.push_back(itClaimRow->second);
                         break;
                     }
                }
            }
        }
    }
    supportQueueNameRowType namedSupportRow;
    if (db.getQueueRow(name, namedSupportRow)) {
        for (supportQueueNameRowType::const_iterator itSupportsForName = namedSupportRow.begin(); itSupportsForName != namedSupportRow.end(); ++itSupportsForName) {
            supportQueueRowType supportRow;
            if (db.getQueueRow(itSupportsForName->nHeight, supportRow)) {
                for (supportQueueRowType::const_iterator itSupportRow = supportRow.begin(); itSupportRow != supportRow.end(); ++itSupportRow) {
                    if (itSupportRow->first == name && itSupportRow->second.outPoint == itSupportsForName->outPoint) {
                        supports.push_back(itSupportRow->second);
                        break;
                    }
                }
            }
        }
    }
    claimsForNameType allClaims(claims, supports, nLastTakeoverHeight);
    return allClaims;
}

//return effective amount from claim, retuns 0 if claim is not found
CAmount CClaimTrie::getEffectiveAmountForClaim(const std::string& name, uint160 claimId) const
{
    std::vector<CSupportValue> supports;
    return getEffectiveAmountForClaimWithSupports(name, claimId, supports);
}

//return effective amount from claim and the supports used as inputs, retuns 0 if claim is not found
CAmount CClaimTrie::getEffectiveAmountForClaimWithSupports(const std::string& name, uint160 claimId,
                                                           std::vector<CSupportValue>& supports) const
{
    claimsForNameType claims = getClaimsForName(name);
    CAmount effectiveAmount = 0;
    bool claim_found = false;
    for (std::vector<CClaimValue>::iterator it=claims.claims.begin(); it!=claims.claims.end(); ++it) {
        if (it->claimId == claimId && it->nValidAtHeight < nCurrentHeight) {
            effectiveAmount += it->nAmount;
            claim_found = true;
            break;
        }
    }
    if (!claim_found) {
        return effectiveAmount;
    }

    for (std::vector<CSupportValue>::iterator it=claims.supports.begin(); it!=claims.supports.end(); ++it) {
        if (it->supportedClaimId == claimId && it->nValidAtHeight < nCurrentHeight) {
            effectiveAmount += it->nAmount;
            supports.push_back(*it);
        }
    }
    return effectiveAmount;
}

bool CClaimTrie::checkConsistency() const
{
    if (empty()) {
        return true;
    } else {
        return recursiveCheckConsistency(&root);
    }
}

bool CClaimTrie::recursiveCheckConsistency(const CClaimTrieNode* node) const
{
    std::vector<unsigned char> vchToHash;

    for (nodeMapType::const_iterator it = node->children.begin(); it != node->children.end(); ++it) {
        if (recursiveCheckConsistency(it->second)) {
            vchToHash.push_back(it->first);
            vchToHash.insert(vchToHash.end(), it->second->hash.begin(), it->second->hash.end());
        } else {
            return false;
        }
    }

    CClaimValue claim;
    bool hasClaim = node->getBestClaim(claim);

    if (hasClaim) {
        uint256 valueHash = getValueHash(claim.outPoint, node->nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    }

    CHash256 hasher;
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Write(vchToHash.data(), vchToHash.size());
    hasher.Finalize(&(vchHash[0]));
    uint256 calculatedHash(vchHash);
    return calculatedHash == node->hash;
}

void CClaimTrie::addToClaimIndex(const std::string& name, const CClaimValue& claim)
{
    CClaimIndexElement element = { name, claim };
    LogPrintf("%s: ClaimIndex[%s] updated %s\n", __func__, claim.claimId.GetHex(), name);

    db.updateQueueRow(claim.claimId, element);
}

void CClaimTrie::removeFromClaimIndex(const CClaimValue& claim)
{
    CClaimIndexElement element;
    db.updateQueueRow(claim.claimId, element);
}

bool CClaimTrie::getClaimById(const uint160 claimId, std::string& name, CClaimValue& claim) const
{
    CClaimIndexElement element;
    if (db.getQueueRow(claimId, element)) {
        name = element.name;
        claim = element.claim;
        return true;
    } else {
        return false;
    }
}

void CClaimTrie::markNodeDirty(const std::string &name, CClaimTrieNode* node)
{
    std::pair<nodeCacheType::iterator, bool> ret;
    ret = dirtyNodes.insert(std::pair<std::string, CClaimTrieNode*>(name, node));
    if (ret.second == false) {
        ret.first->second = node;
    }
}

bool CClaimTrie::updateName(const std::string &name, CClaimTrieNode* updatedNode)
{
    CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname) {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end()) {
            if (itname + 1 == name.end()) {
                CClaimTrieNode* newNode = new CClaimTrieNode();
                current->children[*itname] = newNode;
                current = newNode;
            } else {
                return false;
            }
        } else {
            current = itchild->second;
        }
    }
    assert(current != NULL);
    current->claims.swap(updatedNode->claims);
    markNodeDirty(name, current);
    for (nodeMapType::iterator itchild = current->children.begin(); itchild != current->children.end();) {
        nodeMapType::iterator itupdatechild = updatedNode->children.find(itchild->first);
        if (itupdatechild == updatedNode->children.end()) {
            // This character has apparently been deleted, so delete
            // all descendents from this child.
            std::stringstream ss;
            ss << name << itchild->first;
            std::string newName = ss.str();
            if (!recursiveNullify(itchild->second, newName)) return false;
            current->children.erase(itchild++);
        } else {
            ++itchild;
        }
    }
    return true;
}

bool CClaimTrie::recursiveNullify(CClaimTrieNode* node, std::string& name)
{
    assert(node != NULL);
    for (nodeMapType::iterator itchild = node->children.begin(); itchild != node->children.end(); ++itchild) {
        std::stringstream ss;
        ss << name << itchild->first;
        std::string newName = ss.str();
        if (!recursiveNullify(itchild->second, newName)) return false;
    }
    node->children.clear();
    markNodeDirty(name, NULL);
    delete node;
    return true;
}

bool CClaimTrie::updateHash(const std::string& name, uint256& hash)
{
    CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname) {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end()) return false;
        current = itchild->second;
    }
    assert(current != NULL);
    current->hash = hash;
    markNodeDirty(name, current);
    return true;
}

bool CClaimTrie::updateTakeoverHeight(const std::string& name, int nTakeoverHeight)
{
    CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname) {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end()) return false;
        current = itchild->second;
    }
    assert(current != NULL);
    current->nHeightOfLastTakeover = nTakeoverHeight;
    markNodeDirty(name, current);
    return true;
}

bool CClaimTrie::WriteToDisk()
{
    for (nodeCacheType::iterator itcache = dirtyNodes.begin(); itcache != dirtyNodes.end(); ++itcache)
    {
        CClaimTrieNode *pNode = itcache->second;
        uint32_t num_claims = pNode ? pNode->claims.size() : 0;
        LogPrintf("%s: Writing %s to disk with %d claims\n", __func__, itcache->first, num_claims);
        if (pNode) {
            db.updateQueueRow(itcache->first, *pNode);
        } else {
            CClaimTrieNode emptyNode;
            db.updateQueueRow(itcache->first, emptyNode);
        }
    }

    dirtyNodes.clear();
    db.writeQueues();
    db.Write(HASH_BLOCK, hashBlock);
    db.Write(CURRENT_HEIGHT, nCurrentHeight);
    return db.Sync();
}

bool CClaimTrie::InsertFromDisk(const std::string& name, CClaimTrieNode* node)
{
    if (name.size() == 0) {
        root = *node;
        return true;
    }
    CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname + 1 != name.end(); ++itname) {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end()) return false;
        current = itchild->second;
    }
    current->children[name[name.size()-1]] = new CClaimTrieNode(*node);
    return true;
}

bool CClaimTrie::ReadFromDisk(bool check)
{
    if (!db.Read(HASH_BLOCK, hashBlock))
        LogPrintf("%s: Couldn't read the best block's hash\n", __func__);
    if (!db.Read(CURRENT_HEIGHT, nCurrentHeight))
        LogPrintf("%s: Couldn't read the current height\n", __func__);

    setExpirationTime(Params().GetConsensus().GetExpirationTime(nCurrentHeight-1));

    typedef std::map<std::string, CClaimTrieNode, nodenamecompare> trieNodeMapType;

    trieNodeMapType nodes;
    if(!db.seekByKey(nodes)) {
        return error("%s(): error reading claim trie from disk", __func__);
    }

    for(trieNodeMapType::iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (!InsertFromDisk(it->first, &(it->second))) {
            return error("%s(): error restoring claim trie from disk", __func__);
        }
    }

    if (check) {
        LogPrintf("Checking Claim trie consistency...");
        if (checkConsistency()) {
            LogPrintf("consistent\n");
            return true;
        } else {
            LogPrintf("inconsistent!\n");
            return false;
        }
    }
    return true;
}
