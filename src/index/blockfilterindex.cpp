// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <clientversion.h>
#include <index/blockfilterindex.h>
#include <sqlite.h>
#include <util/system.h>
#include <validation.h>

/* The index database stores three items for each block: the disk location of the encoded filter,
 * its dSHA256 hash, and the header. Those belonging to blocks on the active chain are indexed by
 * height, and those belonging to blocks that have been reorganized out of the active chain are
 * indexed by block hash. This ensures that filter data for any block that becomes part of the
 * active chain can always be retrieved, alleviating timing concerns.
 */

static std::map<BlockFilterType, BlockFilterIndex> g_filter_indexes;

BlockFilterIndex::BlockFilterIndex(BlockFilterType filter_type,
                                   size_t n_cache_size, bool f_memory, bool f_wipe)
    : m_filter_type(filter_type)
{
    const std::string& filter_name = BlockFilterTypeName(filter_type);
    if (filter_name.empty()) throw std::invalid_argument("unknown filter_type");

    fs::path path = GetDataDir() / "filter";
    fs::create_directories(path);
    path /= filter_name;

    m_name = filter_name + " block filter index";
    m_db = MakeUnique<BaseIndex::DB>(path, n_cache_size, f_memory, f_wipe);

    (*m_db) << "CREATE TABLE IF NOT EXISTS block (height INTEGER NOT NULL, hash BLOB NOT NULL COLLATE BINARY, "
               "filter_hash BLOB NOT NULL COLLATE BINARY, header BLOB NOT NULL COLLATE BINARY, "
               "filter_data BLOB NOT NULL COLLATE BINARY, "
               "PRIMARY KEY(height, hash));";

    if (f_wipe) {
        (*m_db) << "DELETE FROM block";
    }
}

bool BlockFilterIndex::WriteBlock(const CBlock& block, const CBlockIndex* pindex)
{
    CBlockUndo block_undo;
    uint256 prev_header;

    if (pindex->nHeight > 0) {
        if (!UndoReadFromDisk(block_undo, pindex))
            return false;

        if (!LookupFilterHeader(pindex->pprev, prev_header))
            return false;
    }

    BlockFilter filter(m_filter_type, block, block_undo);

    const auto filterHash = filter.GetHash(); // trying to avoid temps
    const auto filterHeader = filter.ComputeHeader(prev_header);
    (*m_db) << "INSERT OR REPLACE INTO block VALUES(?, ?, ?, ?, ?)"
            << pindex->nHeight
            << pindex->hash
            << filterHash
            << filterHeader
            << filter.GetEncodedFilter();

    return m_db->rows_modified() > 0;
}

bool BlockFilterIndex::LookupFilter(const CBlockIndex* block_index, BlockFilter& filter_out) const
{
    auto query = *m_db << "SELECT filter_data FROM block WHERE height = ? and hash = ?"
            << block_index->nHeight << block_index->hash;

    for (auto&& row: query) {
        std::vector<uint8_t> data;
        row >> data;
        filter_out = BlockFilter(m_filter_type, block_index->hash, data);
        return true;
    }

    return false;
}

bool BlockFilterIndex::LookupFilterHeader(const CBlockIndex* block_index, uint256& header_out) const
{
    auto query = *m_db << "SELECT header FROM block WHERE height = ? and hash = ?"
                       << block_index->nHeight << block_index->hash;

    for (auto&& row: query) {
        row >> header_out;
        return true;
    }

    return false;
}

bool BlockFilterIndex::LookupFilterRange(int start_height, const CBlockIndex* stop_index,
                                         std::vector<BlockFilter>& filters_out) const
{
    assert(start_height >= 0);
    assert(stop_index->nHeight >= start_height);
    filters_out.reserve(stop_index->nHeight - start_height + 1);
    while(stop_index && stop_index->nHeight >= start_height) {
        filters_out.emplace_back();
        if (!LookupFilter(stop_index, filters_out.back()))
            return false;
        stop_index = stop_index->pprev;
    }
    std::reverse(filters_out.begin(), filters_out.end());
    return true;
}

bool LookupFilterHash(sqlite::database& db, const CBlockIndex* block_index, uint256& hash_out)
{
    auto query = db << "SELECT filter_hash FROM block WHERE height = ? and hash = ?"
                       << block_index->nHeight << block_index->hash;

    for (auto&& row: query) {
        row >> hash_out;
        return true;
    }

    return false;
}

bool BlockFilterIndex::LookupFilterHashRange(int start_height, const CBlockIndex* stop_index,
                                             std::vector<uint256>& hashes_out) const
{
    assert(start_height >= 0);
    assert(stop_index->nHeight >= start_height);
    hashes_out.reserve(stop_index->nHeight - start_height + 1);
    while(stop_index && stop_index->nHeight >= start_height) {
        hashes_out.emplace_back();
        if (!LookupFilterHash(*m_db, stop_index, hashes_out.back()))
            return false;
        stop_index = stop_index->pprev;
    }
    std::reverse(hashes_out.begin(), hashes_out.end());
    return true;
}

BlockFilterIndex* GetBlockFilterIndex(BlockFilterType filter_type)
{
    auto it = g_filter_indexes.find(filter_type);
    return it != g_filter_indexes.end() ? &it->second : nullptr;
}

void ForEachBlockFilterIndex(std::function<void (BlockFilterIndex&)> fn)
{
    for (auto& entry : g_filter_indexes) fn(entry.second);
}

bool InitBlockFilterIndex(BlockFilterType filter_type,
                          size_t n_cache_size, bool f_memory, bool f_wipe)
{
    auto result = g_filter_indexes.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(filter_type),
                                           std::forward_as_tuple(filter_type,
                                                                 n_cache_size, f_memory, f_wipe));
    return result.second;
}

bool DestroyBlockFilterIndex(BlockFilterType filter_type)
{
    return g_filter_indexes.erase(filter_type);
}

void DestroyAllBlockFilterIndexes()
{
    g_filter_indexes.clear();
}
