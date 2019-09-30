#ifndef LBRY_H
#define LBRY_H

#include <chain.h>
#include <chainparams.h>

extern uint32_t g_memfileSize;
unsigned int CalculateLbryNextWorkRequired(const CBlockIndex* pindexLast, int64_t nLastRetargetTime, const Consensus::Params& params);

#endif
