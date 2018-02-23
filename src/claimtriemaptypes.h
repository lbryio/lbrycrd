#ifndef BITCOIN_CLAIMTRIE_MAP_TYPES_H
#define BITCOIN_CLAIMTRIE_MAP_TYPES_H

class CClaimTrieNode;
class CSupportValue;

typedef std::vector<CSupportValue> supportMapEntryType;

typedef std::map<unsigned char, CClaimTrieNode> nodeMapType;

typedef std::pair<std::string, CClaimTrieNode> namedNodeType;

#endif // BITCOIN_CLAIMTRIE_MAP_TYPES_H
