// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"
#include "streams.h"
#include "patternsearch.h"
#include "arith_uint256.h"

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetPoWHash() const
{
    CDataStream ds(SER_GETHASH, PROTOCOL_VERSION);
    ds << *this;
    std::vector<unsigned char> input(ds.begin(), ds.end());

    // Old blocks verify using the old algo
    if(nTime < AES_HARDFORK_TIME)
   	 return PoWHash(input);

    uint256 midHash = Hash(BEGIN(nVersion), END(nNonce));
    if(!patternsearch::pattern_verify( midHash, nStartLocation, nFinalCalculation))
	return uint256S("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    return Hash(BEGIN(nVersion), END(nFinalCalculation));
}

uint256 CBlockHeader::FindBestPatternHash(int& collisions,char *scratchpad,int nThreads) {

        uint256 smallestHashSoFar = uint256S("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        uint32_t smallestHashLocation=0;
        uint32_t smallestHashFinalCalculation=0;


        if(nThreads==0){
            return smallestHashSoFar;
        }

        uint256 midHash = Hash(BEGIN(nVersion), END(nNonce));

        //Threads can only be a power of 2
        int newThreadNumber = 1;
        while(newThreadNumber < nThreads){
            newThreadNumber*=2;
        }
        nThreads=newThreadNumber;


        std::vector< std::pair<uint32_t,uint32_t> > results =patternsearch::pattern_search( midHash,scratchpad,nThreads);
        //uint32_t candidateStartLocation=0;
        //uint32_t candidateFinalCalculation=0;

        collisions=results.size();
        uint256 fullHash = smallestHashSoFar;

        for (unsigned i=0; i < results.size(); i++) {

            nStartLocation = results[i].first;
            nFinalCalculation = results[i].second;
            fullHash = Hash(BEGIN(nVersion), END(nFinalCalculation));
            //LogPrintf("Consider Candidate:%s\n",fullHash.ToString());
            //LogPrintf("against:%s\n",smallestHashSoFar.ToString());

            if(UintToArith256(fullHash)<UintToArith256(smallestHashSoFar)){
                //LogPrintf("New Best Candidate:%s\n",fullHash.ToString());
                //if better, update location
                //printf("best hash so far for the nonce\n");
                smallestHashSoFar=fullHash;
                smallestHashLocation=results[i].first;
                smallestHashFinalCalculation=results[i].second;
            }
        }

        nStartLocation = smallestHashLocation;
        nFinalCalculation = smallestHashFinalCalculation;
        return smallestHashSoFar;
    }

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, hashClaimTrie=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        hashClaimTrie.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i].ToString() << "\n";
    }
    return s.str();
}
