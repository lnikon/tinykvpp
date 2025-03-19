#pragma once

#include <spdlog/spdlog.h>

#include <utility>
#include <vector>

#include "fs/types.h"
#include "wal/common.h"
#include "fs/append_only_file.h"
#include "structures/memtable/memtable.h"
#include "log/log.h"

namespace wal
{

using namespace log;

// trim from start(in place)
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

// TStorage represents the actual storage of logs - it can be simple disk-based log, or a log replicated by Raft
template <TLogConcept TLog> class wal_t
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
    [[nodiscard]] auto records() const -> std::vector<record_t>
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
            if (trim(line).empty())
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

  private:
    TLog m_log;
};

using wal_variant_t =
    std::variant<wal_t<log_t<in_memory_log_storage_t>>, wal_t<log_t<persistent_log_storage_t<file_storage_backend_t>>>>;

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

    auto reset() noexcept -> void
    {
        std::visit([&](auto &wal) { wal.reset(); }, m_wal);
    }

    [[nodiscard]] auto records()
    {
        return std::visit([&](auto &wal) { return wal.records(); }, m_wal);
    }

  private:
    wal_variant_t m_wal;
};

template <TLogConcept<std::string> TLog> using shared_ptr_t = std::shared_ptr<wal_t<TLog>>;

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

template <TLogConcept TLog> auto wal_t<TLog>::operator=(const wal_t &other) -> wal_t &
{
    if (this == &other)
    {
        return *this;
    }
    m_log = other.m_log;
    return *this;
}

template <TLogConcept TLog> auto wal_t<TLog>::operator=(wal_t &&other) noexcept -> wal_t &
{
    if (this == &other)
    {
        return *this;
    }
    m_log = std::move(other.m_log);
    return *this;
}

template <TLogConcept<std::string> TLog> void wal_t<TLog>::add(const record_t &rec) noexcept
{
    std::stringstream strStream;
    rec.write(strStream);
    m_log.append(strStream.str());

    spdlog::debug("Added new WAL entry {}", strStream.str());
}

template <TLogConcept<std::string> TLog> void wal_t<TLog>::reset() noexcept
{
    m_log.reset();
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
        else if constexpr (std::is_same_v<TStorageTag, storage_tags::file_backend_tag>)
        {
            auto log = log_builder_t<storage_tags::file_backend_tag>{}.set_file_path(m_file_path).build();
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
            static_assert(always_false_v<TStorageTag>, "Unsupported storage tag type");
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
