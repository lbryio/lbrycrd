#ifndef BITCOIN_LBRY_H
#define BITCOIN_LBRY_H

#include <chain.h>
#include <chainparams.h>

unsigned int CalculateLbryNextWorkRequired(const CBlockIndex* pindexLast, int64_t nLastRetargetTime, const Consensus::Params& params);

#endif
