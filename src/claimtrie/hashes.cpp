
#include <hashes.h>

// Bitcoin doubles hash
uint256 CalcHash(SHA256_CTX* sha)
{
    uint256 result;
    SHA256_Final(result.begin(), sha);
    SHA256_Init(sha);
    SHA256_Update(sha, result.begin(), result.size());
    SHA256_Final(result.begin(), sha);
    return result;
}

// universal N way hash function
std::function<void(std::vector<uint256>&)> sha256n_way =
    [](std::vector<uint256>& hashes) {
        for (std::size_t i = 0, j = 0; i < hashes.size(); i += 2)
            hashes[j++] = Hash(hashes[i].begin(), hashes[i].end(),
                               hashes[i+1].begin(), hashes[i+1].end());
    };
