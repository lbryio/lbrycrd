
%module(directors="1") libclaimtrie
%{
#include "blob.h"
#include "uints.h"
#include "txoutpoint.h"
#include "data.h"
#include "trie.h"
#include "forks.h"
%}

%feature("flatnested", 1);
%feature("director") CIterateCallback;

%include stl.i
%include stdint.i
%include typemaps.i

%apply int& OUTPUT { int& nValidAtHeight };

%ignore CBaseBlob(CBaseBlob &&);
%ignore CClaimNsupports(CClaimNsupports &&);
%ignore CClaimTrieProof(CClaimTrieProof &&);
%ignore CClaimTrieProofNode(CClaimTrieProofNode &&);
%ignore CClaimValue(CClaimValue &&);
%ignore COutPoint(COutPoint &&);
%ignore CSupportValue(CSupportValue &&);
%ignore uint160(uint160 &&);
%ignore uint256(uint256 &&);

%template(vecUint8) std::vector<uint8_t>;

%include "blob.h"

%template(blob160) CBaseBlob<160>;
%template(blob256) CBaseBlob<256>;

%include "uints.h"
%include "txoutpoint.h"
%include "data.h"

%rename(CClaimTrieCache) CClaimTrieCacheHashFork;

%include "trie.h"
%include "forks.h"

%template(claimEntryType) std::vector<CClaimValue>;
%template(supportEntryType) std::vector<CSupportValue>;
%template(claimsNsupports) std::vector<CClaimNsupports>;

%template(proofPair) std::pair<bool, uint256>;
%template(proofNodePair) std::pair<unsigned char, uint256>;

%template(proofNodes) std::vector<CClaimTrieProofNode>;
%template(proofPairs) std::vector<std::pair<bool, uint256>>;
%template(proofNodeChildren) std::vector<std::pair<unsigned char, uint256>>;

%inline %{
struct CIterateCallback {
    CIterateCallback() = default;
    virtual ~CIterateCallback() = default;
    virtual void apply(const std::string&) = 0;
};

void getNamesInTrie(const CClaimTrieCache& cache, CIterateCallback* cb)
{
    cache.getNamesInTrie([cb](const std::string& name) {
        cb->apply(name);
    });
}
%}

%typemap(in,numinputs=0) CClaimValue&, CClaimTrieProof& %{
    $1 = &$input;
%}
