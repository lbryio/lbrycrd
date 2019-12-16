
#ifndef SQLITE_H
#define SQLITE_H

#include <sqlite/sqlite3.h>
#include <uints.h>

#include <chrono>
#include <thread>

 namespace sqlite
{
    inline int bind_col_in_db(sqlite3_stmt* stmt, int inx, const uint160& val) {
        return sqlite3_bind_blob(stmt, inx, val.begin(), int(val.size()), SQLITE_STATIC);
    }

    inline int bind_col_in_db(sqlite3_stmt* stmt, int inx, const uint256& val) {
        return sqlite3_bind_blob(stmt, inx, val.begin(), int(val.size()), SQLITE_STATIC);
    }

    inline void store_result_in_db(sqlite3_context* db, const uint160& val) {
        sqlite3_result_blob(db, val.begin(), int(val.size()), SQLITE_TRANSIENT);
    }

    inline void store_result_in_db(sqlite3_context* db, const uint256& val) {
        sqlite3_result_blob(db, val.begin(), int(val.size()), SQLITE_TRANSIENT);
    }
}

#include <sqlite/hdr/sqlite_modern_cpp.h>

namespace sqlite
{
    template<>
    struct has_sqlite_type<uint256, SQLITE_BLOB, void> : std::true_type {};

    template<>
    struct has_sqlite_type<uint160, SQLITE_BLOB, void> : std::true_type {};

    inline uint160 get_col_from_db(sqlite3_stmt* stmt, int inx, result_type<uint160>) {
        uint160 ret;
        auto ptr = sqlite3_column_blob(stmt, inx);
        if (!ptr) return ret;
        int bytes = sqlite3_column_bytes(stmt, inx);
        assert(bytes > 0 && bytes <= int(ret.size()));
        std::memcpy(ret.begin(), ptr, bytes);
        return ret;
    }

    inline uint256 get_col_from_db(sqlite3_stmt* stmt, int inx, result_type<uint256>) {
        uint256 ret;
        auto ptr = sqlite3_column_blob(stmt, inx);
        if (!ptr) return ret;
        int bytes = sqlite3_column_bytes(stmt, inx);
        assert(bytes > 0 && bytes <= int(ret.size()));
        std::memcpy(ret.begin(), ptr, bytes);
        return ret;
    }

    inline int commit(database& db, std::size_t attempts = 60)
    {
        int code = SQLITE_OK;
        for (auto i = 0u; i < attempts; ++i) {
            try {
                db << "commit";
            } catch (const sqlite_exception& e) {
                code = e.get_code();
                if (code == SQLITE_LOCKED || code == SQLITE_BUSY) {
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s);
                    continue;
                }
                return code;
            }
            return SQLITE_OK;
        }
        return code;
    }
}

#endif // SQLITE_H
