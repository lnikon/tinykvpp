#pragma once

#include "fs/types.h"
#include <fs/append_only_file.h>
#include <structures/memtable/memtable.h>

#include <spdlog/spdlog.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace db::wal
{

// WAL gets cleaned-up everytime memtable is flushed so its save to reuse the same name
auto wal_filename() -> std::string;

class wal_t
{
  public:
    using kv_t = structures::memtable::memtable_t::record_t;

    enum class operation_k : int8_t
    {
        undefined_k = -1,
        add_k,
        delete_k,
    };

    struct record_t
    {
        operation_k op{operation_k::undefined_k};
        kv_t kv;

        template <typename TStream> void write(TStream &stream) const
        {
            // Write operation opcode
            stream << static_cast<std::int32_t>(op) << ' ';

            // Write key-value pair
            kv.write(stream);
        }

        template <typename TStream> void read(TStream &stream)
        {
            // Read operation opcode
            int32_t opInt{0};
            stream >> opInt;
            op = static_cast<operation_k>(opInt);

            // Read key-value pair
            kv.read(stream);
        }
    };

    /**
     * @brief Constructs a wal_t object with the specified file system path.
     *
     * @param path The file system path where the write-ahead log (WAL) will be stored.
     */
    explicit wal_t(fs::path_t path);

    /**
     * @brief Opens the Write-Ahead Log (WAL) file.
     *
     * This function attempts to open the WAL file specified by the path stored in
     * the member variable `m_path`. It utilizes the `open` method of the `m_log`
     * object to perform the actual file opening operation.
     *
     * @return true if the WAL file was successfully opened, false otherwise.
     */
    auto open() -> bool;

    /**
     * @brief Retrieves the file system path associated with the Write-Ahead Log (WAL).
     *
     * This function returns the path where the WAL is stored.
     *
     * @return fs::path_t The file system path of the WAL.
     */
    auto path() const -> fs::path_t;

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
    void reset();

    /**
     * @brief Recovers the Write-Ahead Log (WAL) by reading and parsing log records.
     *
     * This function attempts to recover the WAL by reading log records from the
     * internal log stream. Each non-empty line in the log stream is parsed into
     * a record_t object, which is then stored in the m_records container. The
     * function logs the start and end of the recovery process, as well as each
     * recovered record for debugging purposes.
     *
     * @return true Always returns true indicating the recovery process completed.
     */
    auto recover() noexcept -> bool;

    /**
     * @brief Retrieves the list of records from the Write-Ahead Log (WAL).
     *
     * This function returns a vector containing all the records currently stored
     * in the WAL. It is a const member function and guarantees not to modify
     * the state of the object.
     *
     * @return std::vector<record_t> A vector containing the records in the WAL.
     */
    auto records() const noexcept -> std::vector<record_t>;

  private:
    fs::path_t m_path;
    std::vector<record_t> m_records;
    fs::append_only_file_t m_log;
};

using shared_ptr_t = std::shared_ptr<wal_t>;

template <typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<wal_t>(std::forward<Args>(args)...);
}

} // namespace db::wal
