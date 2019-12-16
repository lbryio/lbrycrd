
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
