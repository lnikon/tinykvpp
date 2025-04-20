#pragma once

#include <spdlog/spdlog.h>

#include <magic_enum/magic_enum.hpp>

#include <libassert/assert.hpp>

#include <utility>
#include <vector>

#include "fs/types.h"
#include "wal/common.h"
#include "log/log.h"

namespace wal
{

using namespace log;

// TStorage represents the actual storage of logs - it can be simple disk-based
// log, or a log replicated by Raft
template <TLogConcept TLog> class wal_t
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
{
  public:
    /**
     * @brief Constructs a wal_t object with the specified file system path.
     *
     * @param log
     */
    explicit wal_t(TLog log) noexcept;
    wal_t(const wal_t &other);
    wal_t(wal_t &&other) noexcept;
    auto operator=(const wal_t &other) -> wal_t &;
    auto operator=(wal_t &&other) noexcept -> wal_t &;
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
    void add(const record_t &rec) noexcept;

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
    TLog m_log;
};

using wal_variant_t = std::variant<
    wal_t<log_t<in_memory_log_storage_t>>,
    wal_t<log_t<persistent_log_storage_t<file_storage_backend_t>>>>;

class wal_wrapper_t
{
  public:
    template <TLogConcept TLog>
    explicit wal_wrapper_t(wal_t<TLog> wal)
        : m_wal{std::move(wal)}
    {
    }

    auto add(record_t rec) noexcept -> void
    {
        std::visit([&](auto &wal) { return wal.add(std::move(rec)); }, m_wal);
    }

    auto reset() noexcept
    {
        return std::visit([&](auto &wal) { return wal.reset(); }, m_wal);
    }

    [[nodiscard]] auto records()
    {
        return std::visit([&](auto &wal) { return wal.records(); }, m_wal);
    }

  private:
    wal_variant_t m_wal;
};

template <TLogConcept<std::string> TLog>
using shared_ptr_t = std::shared_ptr<wal_t<TLog>>;

template <typename TLog, typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<wal_t<TLog>>(std::forward<Args>(args)...);
}

template <TLogConcept<std::string> TLog>
wal_t<TLog>::wal_t(TLog log) noexcept
    : m_log{std::move(log)}
{
}

template <TLogConcept TLog>
wal_t<TLog>::wal_t(const wal_t &other)
    : m_log(other.m_log)
{
}

template <TLogConcept TLog>
wal_t<TLog>::wal_t(wal_t &&other) noexcept
    : m_log(std::move(other.m_log))
{
}

template <TLogConcept TLog>
auto wal_t<TLog>::operator=(const wal_t &other) -> wal_t &
{
    if (this == &other)
    {
        return *this;
    }
    m_log = other.m_log;
    return *this;
}

template <TLogConcept TLog>
auto wal_t<TLog>::operator=(wal_t &&other) noexcept -> wal_t &
{
    if (this == &other)
    {
        return *this;
    }
    m_log = std::move(other.m_log);
    return *this;
}

template <TLogConcept<std::string> TLog>
void wal_t<TLog>::add(const record_t &rec) noexcept
{
    auto op_view{magic_enum::enum_name(rec.op)};
    ASSERT(m_log.append(std::string{op_view.data(), op_view.size()},
                        rec.kv.m_key.m_key,
                        rec.kv.m_value.m_value),
           "failed to append to WAL");

    spdlog::debug("Added new WAL entry {}", "FILL_ME");
}

template <TLogConcept<std::string> TLog>
auto wal_t<TLog>::reset() noexcept -> bool
{
    return m_log.reset();
}

template <TLogConcept<std::string> TLog>
[[nodiscard]] auto wal_t<TLog>::records() const -> std::vector<record_t>
{
    auto recordToString = [](const record_t &rec)
    {
        std::stringstream strStream;
        rec.write(strStream);
        return strStream.str();
    };

    spdlog::info("wal_t::records() called");
    auto logStream = std::stringstream{};
    for (std::size_t idx{0}; idx < m_log.size(); ++idx)
    {
        if (auto logLine{m_log.read(idx)}; logLine.has_value())
        {
            logStream << logLine.value();
        }
    }

    std::string           line;
    std::vector<record_t> result;
    while (std::getline(logStream, line))
    {
        if (absl::StripAsciiWhitespace(line).empty())
        {
            spdlog::debug("wal::records() empty line after trim. skipping");
            continue;
        }

        std::istringstream lineStream(line);
        record_t           rec;
        rec.read(lineStream);

        spdlog::debug("Recovered WAL record {}", recordToString(rec));

        result.emplace_back(std::move(rec));
    }
    spdlog::info("WAL recovery finished");

    return result;
}

enum class wal_builder_error_t : std::uint8_t
{
    kUndefined,
    kLogBuildFailed,
    kInvalidConfiguration,
};

inline auto to_string(wal_builder_error_t error) noexcept -> std::string_view
{
    switch (error)
    {
    case wal_builder_error_t::kUndefined:
        return {"Undefined"};
    case wal_builder_error_t::kLogBuildFailed:
        return {"LogBuildFailed"};
    case wal_builder_error_t::kInvalidConfiguration:
        return {"InvalidConfiguration"};
    default:
        assert(false);
    }
}

/**
 * @brief Builder for wal_t with different log types
 * @tparam TStorageTag Tag type indicating the storage backend to use
 */
template <typename TStorageTag> class wal_builder_t
{
  public:
    wal_builder_t() = default;

    // File path configuration - only enabled for file backend
    template <typename T = TStorageTag>
        requires std::is_same_v<T, storage_tags::file_backend_tag>
    auto set_file_path(fs::path_t path) -> wal_builder_t &
    {
        m_file_path = std::move(path);
        return *this;
    }

    // Build method that creates the appropriate wal_t instance
    auto build() -> std::expected<wal_wrapper_t, wal_builder_error_t>
    {
        if constexpr (std::is_same_v<TStorageTag, storage_tags::in_memory_tag>)
        {
            auto log = log_builder_t<storage_tags::in_memory_tag>{}.build();
            if (!log.has_value())
            {
                return std::unexpected(wal_builder_error_t::kLogBuildFailed);
            }
            auto wal = wal_t{std::move(log.value())};
            auto wrapper = wal_wrapper_t(std::move(wal));
            return wrapper;
        }
        else if constexpr (std::is_same_v<TStorageTag,
                                          storage_tags::file_backend_tag>)
        {
            auto log = log_builder_t<storage_tags::file_backend_tag>{}
                           .set_file_path(m_file_path)
                           .build();
            if (!log.has_value())
            {
                return std::unexpected(wal_builder_error_t::kLogBuildFailed);
            }
            auto wal = wal_t(std::move(log.value()));
            auto wrapper = wal_wrapper_t(std::move(wal));
            return wrapper;
        }
        else
        {
            static_assert(always_false_v<TStorageTag>,
                          "Unsupported storage tag type");
            return std::unexpected(wal_builder_error_t::kInvalidConfiguration);
        }
    }

  private:
    // Configuration properties
    fs::path_t m_file_path;

    // Helper for static_assert
    template <typename T> static constexpr bool always_false_v = false;
};

/**
 * @brief Factory function to create a WAL builder with in-memory storage
 * @return A builder for wal_t with in-memory storage
 */
inline auto create_in_memory_wal_builder()
{
    return wal_builder_t<storage_tags::in_memory_tag>{};
}

/**
 * @brief Factory function to create a WAL builder with file-based storage
 * @return A builder for wal_t with file-based storage
 */
inline auto create_file_wal_builder()
{
    return wal_builder_t<storage_tags::file_backend_tag>{};
}

} // namespace wal
