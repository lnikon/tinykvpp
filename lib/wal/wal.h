#pragma once

#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>
#include <libassert/assert.hpp>

#include "wal/common.h"
#include "log/log.h"

namespace wal
{

using namespace log;

/**
 * @brief A Write-Ahead Log (WAL) manager for recording and recovering log
 * entries.
 *
 * The wal_t class encapsulates operations for managing a write-ahead log file,
 * including constructing the WAL with a given log interface, adding new
 * records, resetting (i.e., closing, removing, and reopening) the log file, and
 * recovering previously recorded log entries.
 *
 * Constructors:
 *   - wal_t(TLog log) noexcept:
 *       Constructs a wal_t instance using the specified TLog log interface.
 *   - wal_t(const wal_t &other):
 *       Copy constructor. Creates a new instance as a copy of an existing wal_t
 * object.
 *   - wal_t(wal_t &&other) noexcept:
 *       Move constructor. Creates a new instance by acquiring the resources of
 * an existing wal_t object.
 *
 * Assignment Operators:
 *   - operator=(const wal_t &other):
 *       Copy assignment operator. Assigns the content of one wal_t object to
 * another.
 *   - operator=(wal_t &&other) noexcept:
 *       Move assignment operator. Transfers the content from one wal_t object
 * to another.
 *
 * Destructor:
 *   - ~wal_t() = default:
 *       Default destructor.
 *
 * Member Functions:
 *   - void add(const record_t &rec) noexcept:
 *       Appends a new record to the internal records and writes it to the file
 * system. A debug message is logged detailing the content of the record.
 *
 *   - void reset() noexcept:
 *       Resets the WAL by closing the current log file and removing it from the
 * file system. Attempts to reopen the log file and throws a std::runtime_error
 * if reopening fails. Logs an informational message upon successful reset.
 *
 *   - [[nodiscard]] auto records() const -> std::vector<record_t>:
 *       Recovers WAL records by reading, parsing, and processing the log file
 * entries. Reads each log entry into a string stream, processes them line by
 * line, and trims empty lines. For each valid log entry, a record_t instance is
 * constructed and a debug message is logged. Returns a vector containing all
 * successfully recovered WAL records.
 *
 * Logging:
 *   - Uses spdlog for logging informational and debug messages during
 * operations.
 */
class wal_t
{
  public:
    /**
     * @brief Constructs a wal_t object with the specified file system path.
     *
     * @param log
     */
    wal_t() = delete;
    explicit wal_t(log_t log) noexcept;

    wal_t(wal_t &&other) noexcept;
    auto operator=(wal_t &&other) noexcept -> wal_t &;

    wal_t(const wal_t &other) = delete;
    auto operator=(const wal_t &other) -> wal_t & = delete;

    ~wal_t() = default;

    /**
     * @brief Adds a new record to the Write-Ahead Log (WAL).
     *
     * This function appends a new record to the internal list of records and
     * writes the serialized record to the log file. It also logs a debug
     * message with the contents of the new record.
     *
     * @param rec The record to be added to the WAL.
     */
    [[nodiscard]] auto add(const record_t &rec) noexcept -> bool;

    /**
     * @brief Resets the Write-Ahead Log (WAL).
     *
     * This function closes the current log file, removes it from the
     * filesystem, and then attempts to reopen it. If reopening the log file
     * fails, a std::runtime_error is thrown. Upon successful reset, an
     * informational message is logged.
     *
     * @throws std::runtime_error if the log file cannot be reopened.
     */
    auto reset() noexcept -> bool;

    /**
     * @brief Parses and recovers write-ahead log (WAL) records.
     *
     * This method iterates over the underlying log storage to extract and
     * reconstruct WAL records. It reads each log entry into a string stream,
     * processes them line by line, and trims empty lines. For each non-empty
     * line, it constructs a record_t instance by reading from a corresponding
     * string stream and logs debug messages with the recovered record content.
     *
     * @return std::vector<record_t> A vector containing all successfully
     * recovered WAL records.
     *
     * @note The function logs both informational and debug outputs to trace the
     * recovery process.
     */
    [[nodiscard]] auto records() const -> std::vector<record_t>;

  private:
    log_t m_log;
};

using shared_ptr_t = std::shared_ptr<wal_t>;
template <typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<wal_t>(std::forward<Args>(args)...);
}

enum class wal_builder_error_t : std::uint8_t
{
    kUndefined,
    kLogBuildFailed,
    kInvalidConfiguration,
};

/**
 * @brief Builder for wal_t with different log types
 * @tparam TStorageTag Tag type indicating the storage backend to use
 */
class wal_builder_t
{
  public:
    wal_builder_t() = default;

    // Build method that creates the appropriate wal_t instance
    auto build(log_t log) -> std::expected<wal_t, wal_builder_error_t>
    {
        return wal_t(std::move(log));
    }
};

} // namespace wal
