#ifndef BITCOIN_CLAIMTRIE_TYPEDEFS_H
#define BITCOIN_CLAIMTRIE_TYPEDEFS_H

class CClaimTrieNode;
class CSupportValue;
class CClaimValue;
struct outPointHeightType;
struct nameOutPointHeightType;
struct nameOutPoint;
struct nameOutPointType;
struct nodenamecompare;
class COutPoint;

typedef std::vector<CSupportValue> supportMapEntryType;

typedef std::map<unsigned char, CClaimTrieNode> nodeMapType;

typedef std::pair<std::string, CClaimTrieNode> namedNodeType;

typedef std::pair<std::string, CClaimValue> claimQueueEntryType;

typedef std::pair<std::string, CSupportValue> supportQueueEntryType;

typedef std::map<std::string, supportMapEntryType> supportMapType;

typedef std::vector<outPointHeightType> queueNameRowType;
typedef std::map<std::string, queueNameRowType> queueNameType;

typedef std::vector<nameOutPointHeightType> insertUndoType;

typedef std::vector<nameOutPointType> expirationQueueRowType;
typedef std::map<int, expirationQueueRowType> expirationQueueType;

typedef std::vector<claimQueueEntryType> claimQueueRowType;
typedef std::map<int, claimQueueRowType> claimQueueType;

typedef std::vector<supportQueueEntryType> supportQueueRowType;
typedef std::map<int, supportQueueRowType> supportQueueType;

typedef std::map<std::string, CClaimTrieNode, nodenamecompare> nodeCacheType;

typedef std::map<std::string, uint256> hashMapType;

#endif // BITCOIN_CLAIMTRIE_TYPEDEFS_H
