// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/pureheader.h>

#include <hash.h>
#include <streams.h>

uint256 CPureBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CPureBlockHeader::GetPoWHash() const
{
    CDataStream ds(SER_GETHASH, PROTOCOL_VERSION);
    ds << *this;
    auto once = Hash(ds.begin(), ds.end());
    return Hash(once.begin(), once.end());
}