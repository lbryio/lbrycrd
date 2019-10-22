
#ifndef CLAIMTRIE_FORKS_H
#define CLAIMTRIE_FORKS_H

#include <trie.h>

class CClaimTrieCacheExpirationFork : public CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheExpirationFork(CClaimTrie* base);

    int expirationTime() const override;

    virtual void initializeIncrement();
    bool finalizeDecrement(takeoverUndoType& takeoverHeightUndo) override;

    bool incrementBlock(insertUndoType& insertUndo,
                        claimUndoType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportUndoType& expireSupportUndo,
                        takeoverUndoType& takeoverHeightUndo) override;

    bool decrementBlock(insertUndoType& insertUndo,
                        claimUndoType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportUndoType& expireSupportUndo) override;

protected:
    int expirationHeight;

private:
    bool forkForExpirationChange(bool increment);
};

class CClaimTrieCacheNormalizationFork : public CClaimTrieCacheExpirationFork
{
public:
    explicit CClaimTrieCacheNormalizationFork(CClaimTrie* base);

    bool shouldNormalize() const;

    // lower-case and normalize any input string name
    // see: https://unicode.org/reports/tr15/#Norm_Forms
    std::string normalizeClaimName(const std::string& name, bool force = false) const; // public only for validating name field on update op

    bool incrementBlock(insertUndoType& insertUndo,
                        claimUndoType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportUndoType& expireSupportUndo,
                        takeoverUndoType& takeoverHeightUndo) override;

    bool decrementBlock(insertUndoType& insertUndo,
                        claimUndoType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportUndoType& expireSupportUndo) override;

    bool getProofForName(const std::string& name, const CUint160& claim, CClaimTrieProof& proof) override;
    bool getInfoForName(const std::string& name, CClaimValue& claim, int heightOffset = 0) const override;
    CClaimSupportToName getClaimsForName(const std::string& name) const override;
    std::string adjustNameForValidHeight(const std::string& name, int validHeight) const override;

protected:
    int getDelayForName(const std::string& name, const CUint160& claimId) const override;

private:
    bool normalizeAllNamesInTrieIfNecessary(takeoverUndoType& takeovers);
    bool unnormalizeAllNamesInTrieIfNecessary();
};

class CClaimTrieCacheHashFork : public CClaimTrieCacheNormalizationFork
{
public:
    explicit CClaimTrieCacheHashFork(CClaimTrie* base);

    bool getProofForName(const std::string& name, const CUint160& claim, CClaimTrieProof& proof) override;
    void initializeIncrement() override;
    bool finalizeDecrement(takeoverUndoType& takeoverHeightUndo) override;

    bool allowSupportMetadata() const;

protected:
    CUint256 recursiveComputeMerkleHash(const std::string& name, int takeoverHeight, bool checkOnly) override;
};

typedef CClaimTrieCacheHashFork CClaimTrieCache;

#endif // CLAIMTRIE_FORKS_H
