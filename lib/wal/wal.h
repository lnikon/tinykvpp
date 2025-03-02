#pragma once

#include "fs/types.h"
#include "wal/common.h"
#include "fs/append_only_file.h"
#include "structures/memtable/memtable.h"
#include "concepts.h"
#include "in_memory_log_storage.h"
#include "persistent_log_storage.h"
#include "log.h"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>
#include <vector>

namespace wal
{

// WAL gets cleaned-up everytime memtable is flushed so its save to reuse the same name
auto wal_filename() -> std::string;

// TStorage represents the actual storage of logs - it can be simple disk-based log, or a log replcated by Raft
template <TLogConcept TLog> class wal_t
{
  public:
    /**
     * @brief Constructs a wal_t object with the specified file system path.
     *
     * @param path The file system path where the write-ahead log (WAL) will be stored.
     */
    explicit wal_t(TLog &&log) noexcept;

    /**
     * @brief Adds a new record to the Write-Ahead Log (WAL).
     *
     * This function appends a new record to the internal list of records and writes
     * the serialized record to the log file. It also logs a debug message with the
     * contents of the new record.
     *
     * @param rec The record to be added to the WAL.
     */
    void add(const record_t &rec) noexcept;

    /**
     * @brief Resets the Write-Ahead Log (WAL).
     *
     * This function closes the current log file, removes it from the filesystem,
     * and then attempts to reopen it. If reopening the log file fails, a
     * std::runtime_error is thrown. Upon successful reset, an informational
     * message is logged.
     *
     * @throws std::runtime_error if the log file cannot be reopened.
     */
    void reset() noexcept;

    /**
     * @brief Recovers the Write-Ahead Log (WAL) by reading and parsing log records.
     *
     * This function attempts to recover the WAL by reading log records from the
     * internal log stream. Each non-empty line in the log stream is parsed into
     * a record_t object, which is then stored in the m_log container. The
     * function logs the start and end of the recovery process, as well as each
     * recovered record for debugging purposes.
     *
     * @return true Always returns true indicating the recovery process completed.
     */
    auto recover() noexcept -> bool;

  private:
    TLog m_log;
};

using wal_variant_t =
    std::variant<wal_t<log_t<persistent_log_storage_t<file_storage_backend_t>>>, wal_t<log_t<in_memory_log_storage_t>>>;

class wal_wrapper_t
{
  public:
    template <TLogConcept TLog>
    explicit wal_wrapper_t(wal_t<TLog> &&wal)
        : m_wal(std::move(wal))
    {
    }

    auto add(record_t rec) noexcept -> void
    {
        std::visit([&](auto &wal) { return wal.add(std::move(rec)); }, m_wal);
    }

    auto reset() noexcept -> void
    {
        std::visit([&](auto &wal) { wal.reset(); }, m_wal);
    }

    [[nodiscard]] auto recover() noexcept -> bool
    {
        return std::visit([&](auto &wal) { return wal.recover(); }, m_wal);
    }

  private:
    wal_variant_t m_wal;
};

template <TLogConcept<std::string> TLog> using shared_ptr_t = std::shared_ptr<wal_t<TLog>>;

template <typename TLog, typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<wal_t<TLog>>(std::forward<Args>(args)...);
}

inline auto wal_filename() -> std::string
{
    return {"wal"};
}

template <TLogConcept<std::string> TLog>
wal_t<TLog>::wal_t(TLog &&log) noexcept
    : m_log{std::move(log)}
{
}

template <TLogConcept<std::string> TLog> void wal_t<TLog>::add(const record_t &rec) noexcept
{
    std::stringstream strStream;
    rec.write(strStream);
    m_log.append(strStream.str());

    spdlog::debug("Added new WAL entry {}", strStream.str());
}

template <TLogConcept<std::string> TLog> void wal_t<TLog>::reset()
{
    throw std::runtime_error("wal RESET logic should be reimplemented");
    // fs::stdfs::remove(m_path);
    // if (!m_rLog.open(m_path))
    // {
    //     throw std::runtime_error("unable to reset wal " + m_path.string());
    // }
    //
    // spdlog::info("wal reset is successful " + m_path.string());
}

// trim from start (in place)
inline void ltrim(std::string &str)
{
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

inline auto trim(std::string &s) -> std::string &
{
    rtrim(s);
    ltrim(s);
    return s;
}

template <TLogConcept<std::string> TLog> auto wal_t<TLog>::recover() noexcept -> bool
{
    throw std::runtime_error("Wal recovery logic should be moved into log_t<TLogStorageConcept>");

    // auto recordToString = [](const record_t &rec)
    // {
    //     std::stringstream strStream;
    //     rec.write(strStream);
    //     return strStream.str();
    // };
    //
    // spdlog::info("WAL recovery started");
    // auto        stringStream = m_log.stream();
    // std::string line;
    // while (std::getline(stringStream, line))
    // {
    //     if (trim(line).empty())
    //     {
    //         continue;
    //     }
    //
    //     std::istringstream lineStream(line);
    //     record_t           rec;
    //     rec.read(lineStream);
    //
    //     m_log.append(rec);
    //
    //     spdlog::debug("Recovered WAL record {}", recordToString(rec));
    // }
    // spdlog::info("WAL recovery finished");
    //
    // return true;
}

} // namespace wal
