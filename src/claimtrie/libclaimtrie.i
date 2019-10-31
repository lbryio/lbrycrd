
%module libclaimtrie
%{
#include "uints.h"
#include "txoutpoint.h"
#include "data.h"
#include "trie.h"
#include "forks.h"
%}

%feature("directors", 1);
%feature("flatnested", 1);

%include stl.i
%include stdint.i
%include std_pair.i

%ignore CBaseBlob(CBaseBlob &&);
%ignore CClaimIndexElement(CClaimIndexElement &&);
%ignore CClaimNsupports(CClaimNsupports &&);
%ignore CClaimTrieProof(CClaimTrieProof &&);
%ignore CClaimTrieProofNode(CClaimTrieProofNode &&);
%ignore CClaimValue(CClaimValue &&);
%ignore CSupportValue(CSupportValue &&);
%ignore CTxOutPoint(CTxOutPoint &&);

#define SWIG_INTERFACE

%include "uints.h"
%include "txoutpoint.h"
%include "data.h"
%include "trie.h"
%include "forks.h"

%template(CUint160) CBaseBlob<160>;
%template(CUint256) CBaseBlob<256>;
%template(uint8vec) std::vector<uint8_t>;

%template(claimEntryType) std::vector<CClaimValue>;
%template(supportEntryType) std::vector<CSupportValue>;
%template(claimsNsupports) std::vector<CClaimNsupports>;

%template(proofPair) std::pair<bool, CUint256>;
%template(proofNodePair) std::pair<unsigned char, CUint256>;
%template(claimUndoPair) std::pair<std::string, CClaimValue>;
%template(supportUndoPair) std::pair<std::string, CSupportValue>;
%template(takeoverUndoPair) std::pair<std::string, <std::pair<int, CUint160>>;

%template(proofNodes) std::vector<CClaimTrieProofNode>;
%template(proofPairs) std::vector<std::pair<bool, CUint256>>;
%template(proofNodeChildren) std::vector<std::pair<unsigned char, CUint256>>;
%template(claimUndoType) std::vector<claimUndoPair>;
%template(supportUndoType) std::vector<supportUndoPair>;
%template(insertUndoType) std::vector<CNameOutPointHeightType>;
%template(takeoverUndoType) std::vector<takeoverUndoPair>;

%rename(CClaimTrieCache) CClaimTrieCacheHashFork;
