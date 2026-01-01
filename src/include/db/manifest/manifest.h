#pragma once

#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <variant>
#include <cstdint>

#include <spdlog/spdlog.h>

#include "fs/types.h"
#include "wal/wal.h"

namespace db::manifest
{

struct manifest_t
{
    using level_index_t = std::uint64_t;
    using segment_name_t = std::string;

    enum record_type_k : int8_t
    {
        segment_k = 0,
        level_k,
    };

    struct segment_record_t
    {
        enum class operation_k : int8_t
        {
            undefined_k = -1,
            add_segment_k = 0,
            remove_segment_k = 1,
        };

        [[nodiscard]] static auto ToString(operation_k operation) -> std::string
        {
            switch (operation)
            {
            case operation_k::add_segment_k:
                return {"add_segment_k"};
            case operation_k::remove_segment_k:
                return {"remove_segment_k"};
            default:
                return {"unkown op"};
            }
        }

        [[nodiscard]] auto ToString() const -> std::string
        {
            std::stringstream stringStream;
            write(stringStream);
            return stringStream.str();
        }

        template <typename stream_gt> void write(stream_gt &outStream) const
        {
            outStream << static_cast<std::int32_t>(type) << ' ' << static_cast<std::int32_t>(op)
                      << ' ' << name << ' ' << level << std::endl;
        }

        /**
         * @brief Serialize manifest segment record into stream.
         *        Format:
         * <operation-type><whitespace><segment-name><whitespace><level-index>
         *                |int        |char         |string     |char |int
         *
         * @tparam stream_gt
         * @param os
         */
        template <typename stream_gt> void read(stream_gt &outStream)
        {
            // Deserialize operation
            std::int32_t op_int{0};
            outStream >> op_int;
            op = static_cast<operation_k>(op_int);

            // Deserialize name of the segment
            outStream >> name;

            // Deserialize level of the segment
            outStream >> level;
        }

        record_type_k  type{record_type_k::segment_k};
        operation_k    op{operation_k::undefined_k};
        segment_name_t name;
        level_index_t  level{};
    };

    struct level_record_t
    {
        enum class operation_k : int8_t
        {
            undefined_k = -1,
            add_level_k,
            compact_level_k,
            purge_level_k,
        };

        [[nodiscard]] static auto ToString(operation_k operation) -> std::string
        {
            switch (operation)
            {
            case operation_k::add_level_k:
                return {"add_level_k"};
            case operation_k::compact_level_k:
                return {"compact_level_k"};
            case operation_k::purge_level_k:
                return {"purge_level_k"};
            default:
                return {"unkown op"};
            }
        }

        [[nodiscard]] auto ToString() const -> std::string
        {
            std::stringstream stringStream;
            write(stringStream);
            return stringStream.str();
        }

        /**
         * @brief Serialize manifest level record into stream.
         *        Format: <operation-type><whitespace><level-index>
         *                |int            |char       |int
         *
         * @tparam stream_gt
         * @param os
         */
        template <typename stream_gt> void write(stream_gt &outStream) const
        {
            outStream << static_cast<std::int32_t>(type) << ' ' << static_cast<std::int32_t>(op)
                      << ' ' << level;
        }

        template <typename stream_gt> void read(stream_gt &outStream)
        {
            // Deserialize operation
            std::int32_t op_int{0};
            outStream >> op_int;
            op = static_cast<operation_k>(op_int);

            // Deserialize level
            outStream >> level;
        }

        record_type_k type{record_type_k::level_k};
        operation_k   op{operation_k::undefined_k};
        level_index_t level{};
    };

    using record_t = std::variant<std::monostate, segment_record_t, level_record_t>;
    using storage_t = std::vector<record_t>;

    manifest_t(fs::path_t path, wal::shared_ptr_t<manifest_t::record_t> pWal) noexcept;

    [[nodiscard]] auto path() const noexcept -> fs::path_t;

    [[nodiscard]] auto add(record_t info) -> bool;
    [[nodiscard]] auto records() const noexcept -> storage_t;

    void enable();
    void disable();

  private:
    bool                                    m_enabled{false};
    fs::path_t                              m_path;
    wal::shared_ptr_t<manifest_t::record_t> m_pWal;
};

using shared_ptr_t = std::shared_ptr<manifest_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<manifest_t>(std::forward<Args>(args)...);
}

template <typename TStream>
auto operator<<(TStream &stream, const manifest_t::record_t &crRecord) -> TStream &
{
    std::visit(
        [&stream](const auto &record)
        {
            if constexpr (std::is_same_v<std::monostate, std::decay_t<decltype(record)>>)
            {
                spdlog::error("manifest_t::record_t: Attempt to write uninitialized record.");
            }
            else
            {
                record.write(stream);
            }
        },
        crRecord
    );
    return stream;
}

template <typename TStream>
auto operator>>(TStream &stream, manifest_t::record_t &rRecord) -> TStream &
{
    int record_type_int{0};
    stream >> record_type_int;

    switch (static_cast<manifest_t::record_type_k>(record_type_int))
    {
    case manifest_t::record_type_k::segment_k:
    {
        manifest_t::segment_record_t record;
        record.read(stream);
        rRecord.emplace<1>(std::move(record));
        break;
    }
    case manifest_t::record_type_k::level_k:
    {
        manifest_t::level_record_t record;
        record.read(stream);
        rRecord.emplace<2>(record);
        break;
    }
    default:
    {
        spdlog::error("unhandled record_type_int={}. Skipping record.", record_type_int);
        break;
    }
    }
    return stream;
}

struct manifest_builder_t final
{
    [[nodiscard]] auto build(fs::path_t path, wal::shared_ptr_t<manifest_t::record_t> wal)
        -> std::optional<manifest_t>;
};

} // namespace db::manifest
