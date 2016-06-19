#include "lbry.h" 
#include "uint256.h" 
#include <cstdio>
unsigned int CalculateLbryNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    const int64_t retargetTimespan = params.nPowTargetTimespan;
    const int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    int64_t nModulatedTimespan = nActualTimespan;
    int64_t nMaxTimespan;
    int64_t nMinTimespan;

    nModulatedTimespan = retargetTimespan + (nModulatedTimespan - retargetTimespan) / 8;

    nMinTimespan = retargetTimespan - (retargetTimespan / 8); //(150 - 18 = 132)
    nMaxTimespan = retargetTimespan + (retargetTimespan / 2); //(150 + 75 = 225) 

    // Limit adjustment step
    if (nModulatedTimespan < nMinTimespan)
        nModulatedTimespan = nMinTimespan;
    else if (nModulatedTimespan > nMaxTimespan)
        nModulatedTimespan = nMaxTimespan;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    bnNew *= nModulatedTimespan;
    bnNew /= retargetTimespan;
    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}


