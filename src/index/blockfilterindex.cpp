// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <clientversion.h>
#include <index/blockfilterindex.h>
#include <streams.h>
#include <sqlite.h>
#include <util/system.h>
#include <validation.h>

/* The index database stores three items for each block: the disk location of the encoded filter,
 * its dSHA256 hash, and the header. Those belonging to blocks on the active chain are indexed by
 * height, and those belonging to blocks that have been reorganized out of the active chain are
 * indexed by block hash. This ensures that filter data for any block that becomes part of the
 * active chain can always be retrieved, alleviating timing concerns.
 */

constexpr unsigned int MAX_FLTR_FILE_SIZE = 0x1000000; // 16 MiB
/** The pre-allocation chunk size for fltr?????.dat files */
constexpr unsigned int FLTR_FILE_CHUNK_SIZE = 0x100000; // 1 MiB

namespace {

struct DBVal {
    uint256 hash;
    uint256 header;
    FlatFilePos pos;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(header);
        READWRITE(pos);
    }
};

} // namespace

static std::map<BlockFilterType, BlockFilterIndex> g_filter_indexes;

BlockFilterIndex::BlockFilterIndex(BlockFilterType filter_type,
                                   size_t n_cache_size, bool f_memory, bool f_wipe)
    : m_filter_type(filter_type)
{
    const std::string& filter_name = BlockFilterTypeName(filter_type);
    if (filter_name.empty()) throw std::invalid_argument("unknown filter_type");

    fs::path path = GetDataDir() / "indexes" / "blockfilter" / filter_name;
    fs::create_directories(path);

    m_name = filter_name + " block filter index";
    m_db = MakeUnique<BaseIndex::DB>(path / "db", n_cache_size, f_memory, f_wipe);
    m_filter_fileseq = MakeUnique<FlatFileSeq>(std::move(path), "fltr", FLTR_FILE_CHUNK_SIZE);
}

bool BlockFilterIndex::Init()
{
    m_db->ReadFilePos(m_next_filter_pos);
    return BaseIndex::Init();
}

bool BlockFilterIndex::CommitInternal()
{
    const FlatFilePos& pos = m_next_filter_pos;

    // Flush current filter file to disk.
    CAutoFile file(m_filter_fileseq->Open(pos), SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        return error("%s: Failed to open filter file %d", __func__, pos.nFile);
    }
    if (!FileCommit(file.Get())) {
        return error("%s: Failed to commit filter file %d", __func__, pos.nFile);
    }

    return BaseIndex::CommitInternal();
}

bool BlockFilterIndex::ReadFilterFromDisk(const FlatFilePos& pos, BlockFilter& filter) const
{
    CAutoFile filein(m_filter_fileseq->Open(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        return false;
    }

    uint256 block_hash;
    std::vector<unsigned char> encoded_filter;
    try {
        filein >> block_hash >> encoded_filter;
        filter = BlockFilter(GetFilterType(), block_hash, std::move(encoded_filter));
    }
    catch (const std::exception& e) {
        return error("%s: Failed to deserialize block filter from disk: %s", __func__, e.what());
    }

    return true;
}

size_t BlockFilterIndex::WriteFilterToDisk(FlatFilePos& pos, const BlockFilter& filter)
{
    assert(filter.GetFilterType() == GetFilterType());

    size_t data_size =
        GetSerializeSize(filter.GetBlockHash(), CLIENT_VERSION) +
        GetSerializeSize(filter.GetEncodedFilter(), CLIENT_VERSION);

    // If writing the filter would overflow the file, flush and move to the next one.
    if (pos.nPos + data_size > MAX_FLTR_FILE_SIZE) {
        CAutoFile last_file(m_filter_fileseq->Open(pos), SER_DISK, CLIENT_VERSION);
        if (last_file.IsNull()) {
            LogPrintf("%s: Failed to open filter file %d\n", __func__, pos.nFile);
            return 0;
        }
        if (!TruncateFile(last_file.Get(), pos.nPos)) {
            LogPrintf("%s: Failed to truncate filter file %d\n", __func__, pos.nFile);
            return 0;
        }
        if (!FileCommit(last_file.Get())) {
            LogPrintf("%s: Failed to commit filter file %d\n", __func__, pos.nFile);
            return 0;
        }

        pos.nFile++;
        pos.nPos = 0;
    }

    // Pre-allocate sufficient space for filter data.
    bool out_of_space;
    m_filter_fileseq->Allocate(pos, data_size, out_of_space);
    if (out_of_space) {
        LogPrintf("%s: out of disk space\n", __func__);
        return 0;
    }

    CAutoFile fileout(m_filter_fileseq->Open(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull()) {
        LogPrintf("%s: Failed to open filter file %d\n", __func__, pos.nFile);
        return 0;
    }

    fileout << filter.GetBlockHash() << filter.GetEncodedFilter();
    return data_size;
}

bool BlockFilterIndex::WriteBlock(const CBlock& block, const CBlockIndex* pindex)
{
    CBlockUndo block_undo;
    uint256 prev_header;

    if (pindex->nHeight > 0) {
        if (!UndoReadFromDisk(block_undo, pindex)) {
            return false;
        }

        uint256 block_hash;
        auto query = (*m_db) << "SELECT hash, header FROM block WHERE height = ?"
                             << pindex->nHeight - 1;
        auto it = query.begin();
        if (it == query.end())
            return false;

        *it >> block_hash >> prev_header;

        uint256 expected_block_hash = pindex->pprev->GetBlockHash();
        if (block_hash != expected_block_hash) {
            return error("%s: previous block header belongs to unexpected block %s; expected %s",
                         __func__, block_hash.ToString(), expected_block_hash.ToString());
        }
    }

    BlockFilter filter(m_filter_type, block, block_undo);

    size_t bytes_written = WriteFilterToDisk(m_next_filter_pos, filter);
    if (bytes_written == 0) return false;

    (*m_db) << "INSERT INTO block VALUES(?, ?, ?, ?, ?, ?)"
            << pindex->nHeight
            << pindex->GetBlockHash()
            << filter.GetHash()
            << filter.ComputeHeader(prev_header)
            << m_next_filter_pos.nFile
            << m_next_filter_pos.nPos;

    if (!(m_db->rows_modified() > 0)) {
        return false;
    }

    m_next_filter_pos.nPos += bytes_written;
    return true;
}

static bool CopyHeightIndexToHashIndex(sqlite::database& db,
                                       int start_height, int stop_height)
{
    db << "UPDATE block SET height = NULL WHERE height >= ? and height <= ?"
        << start_height << stop_height;
    return db.rows_modified() > 0;
}

bool BlockFilterIndex::Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip)
{
    assert(current_tip->GetAncestor(new_tip->nHeight) == new_tip);

    // During a reorg, we need to copy all filters for blocks that are getting disconnected from the
    // height index to the hash index so we can still find them when the height index entries are
    // overwritten.
    if (!CopyHeightIndexToHashIndex(*m_db, new_tip->nHeight, current_tip->nHeight)) {
        return false;
    }

    // The latest filter position gets written in Commit by the call to the BaseIndex::Rewind.
    // But since this creates new references to the filter, the position should get updated here
    // atomically as well in case Commit fails.
    if (!m_db->WriteFilePos(m_next_filter_pos)) return false;

    return BaseIndex::Rewind(current_tip, new_tip);
}

static bool LookupOne(sqlite::database& db, const CBlockIndex* block_index, DBVal& result)
{
    // First check if the result is stored under the height index and the value there matches the
    // block hash. This should be the case if the block is on the active chain.
    auto query = db << "SELECT filter_hash, header, file, pos FROM block WHERE (height = ? "
                        "OR height IS NULL) AND hash = ? LIMIT 1"
                    << block_index->nHeight << block_index->GetBlockHash();

    for (auto&& row : query) {
        row >> result.hash >> result.header >> result.pos.nFile >> result.pos.nPos;
        return true;
    }
    return false;
}

static bool LookupRange(sqlite::database& db, const std::string& index_name, int start_height,
                        const CBlockIndex* stop_index, std::vector<DBVal>& results)
{
    if (start_height < 0) {
        return error("%s: start height (%d) is negative", __func__, start_height);
    }
    if (start_height > stop_index->nHeight) {
        return error("%s: start height (%d) is greater than stop height (%d)",
                     __func__, start_height, stop_index->nHeight);
    }

    size_t results_size = static_cast<size_t>(stop_index->nHeight - start_height + 1);
    results.resize(results_size);

    // Iterate backwards through block indexes collecting results in order to access the block hash
    // of each entry in case we need to look it up in the hash index.
    for (const CBlockIndex* block_index = stop_index;
         block_index && block_index->nHeight >= start_height;
         block_index = block_index->pprev) {
        size_t i = static_cast<size_t>(block_index->nHeight - start_height);
        if (!LookupOne(db, block_index, results[i])) {
            return error("%s: unable to read value in %s at key %s", __func__,
                         index_name, block_index->GetBlockHash().ToString());
        }
    }

    return true;
}

bool BlockFilterIndex::LookupFilter(const CBlockIndex* block_index, BlockFilter& filter_out) const
{
    DBVal entry;
    if (!LookupOne(*m_db, block_index, entry)) {
        return false;
    }

    return ReadFilterFromDisk(entry.pos, filter_out);
}

bool BlockFilterIndex::LookupFilterHeader(const CBlockIndex* block_index, uint256& header_out) const
{
    DBVal entry;
    if (!LookupOne(*m_db, block_index, entry)) {
        return false;
    }

    header_out = entry.header;
    return true;
}

bool BlockFilterIndex::LookupFilterRange(int start_height, const CBlockIndex* stop_index,
                                         std::vector<BlockFilter>& filters_out) const
{
    std::vector<DBVal> entries;
    if (!LookupRange(*m_db, m_name, start_height, stop_index, entries)) {
        return false;
    }

    filters_out.resize(entries.size());
    auto filter_pos_it = filters_out.begin();
    for (const auto& entry : entries) {
        if (!ReadFilterFromDisk(entry.pos, *filter_pos_it)) {
            return false;
        }
        ++filter_pos_it;
    }

    return true;
}

bool BlockFilterIndex::LookupFilterHashRange(int start_height, const CBlockIndex* stop_index,
                                             std::vector<uint256>& hashes_out) const

{
    std::vector<DBVal> entries;
    if (!LookupRange(*m_db, m_name, start_height, stop_index, entries)) {
        return false;
    }

    hashes_out.clear();
    hashes_out.reserve(entries.size());
    for (const auto& entry : entries) {
        hashes_out.push_back(entry.hash);
    }
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
