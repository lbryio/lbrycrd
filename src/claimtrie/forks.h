
#ifndef CLAIMTRIE_FORKS_H
#define CLAIMTRIE_FORKS_H

#include <trie.h>

class CClaimTrieCacheExpirationFork : public CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheExpirationFork(CClaimTrie* base);
    CClaimTrieCacheExpirationFork(CClaimTrieCacheExpirationFork&&) = default;

    int expirationTime() const override;

    virtual void initializeIncrement();
    bool incrementBlock() override;
    bool decrementBlock() override;
    bool finalizeDecrement() override;

protected:
    int expirationHeight;

private:
    bool forkForExpirationChange(bool increment);
};

class CClaimTrieCacheNormalizationFork : public CClaimTrieCacheExpirationFork
{
public:
    explicit CClaimTrieCacheNormalizationFork(CClaimTrie* base);
    CClaimTrieCacheNormalizationFork(CClaimTrieCacheNormalizationFork&&) = default;

    bool shouldNormalize() const;

    // lower-case and normalize any input string name
    // see: https://unicode.org/reports/tr15/#Norm_Forms
    std::string normalizeClaimName(const std::string& name, bool force = false) const; // public only for validating name field on update op

    bool incrementBlock() override;
    bool decrementBlock() override;

    bool getProofForName(const std::string& name, const uint160& claim, CClaimTrieProof& proof) override;
    bool getInfoForName(const std::string& name, CClaimValue& claim, int heightOffset = 0) override;

    CClaimSupportToName getClaimsForName(const std::string& name) const override;
    std::string adjustNameForValidHeight(const std::string& name, int validHeight) const override;

protected:
    int getDelayForName(const std::string& name, const uint160& claimId) const override;

private:
    bool normalizeAllNamesInTrieIfNecessary();
    bool unnormalizeAllNamesInTrieIfNecessary();
};

class CClaimTrieCacheHashFork : public CClaimTrieCacheNormalizationFork
{
public:
    explicit CClaimTrieCacheHashFork(CClaimTrie* base);
    CClaimTrieCacheHashFork(CClaimTrieCacheHashFork&&) = default;

    bool getProofForName(const std::string& name, const uint160& claim, CClaimTrieProof& proof) override;
    void initializeIncrement() override;
    bool finalizeDecrement() override;

    bool allowSupportMetadata() const;

protected:
    uint256 computeNodeHash(const std::string& name, int takeoverHeight) override;
};

typedef CClaimTrieCacheHashFork CClaimTrieCache;

#endif // CLAIMTRIE_FORKS_H
