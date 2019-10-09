#ifndef LBRY_H
#define LBRY_H

#include <chain.h>
#include <chainparams.h>

unsigned int CalculateLbryNextWorkRequired(const CBlockIndex* pindexLast, int64_t nLastRetargetTime, const Consensus::Params& params);

#endif
