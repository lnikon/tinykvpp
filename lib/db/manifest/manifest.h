#pragma once

#include "config/config.h"
#include "fs/types.h"
#include <fs/append_only_file.h>

#include <spdlog/spdlog.h>

#include <sstream>
#include <string>
#include <vector>
#include <variant>
#include <cstdint>

namespace db::manifest
{

/**
 * @class manifest_t
 * @brief Manipulate disk-level manifest file
 */
struct manifest_t
{
    using level_index_t = std::size_t;
    using segment_name_t = std::string;

    enum record_type_k : int8_t
    {
        segment_k = 0,
        level_k,
    };

    /**
     * @class segment_record_t
     * @brief Represents an operation applicable to segment
     *
     */
    struct segment_record_t
    {
        enum class operation_k : int8_t
        {
            undefined_k = -1,
            add_segment_k = 0,
            remove_segment_k,
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
            outStream << static_cast<std::int32_t>(type) << ' ' << static_cast<std::int32_t>(op) << ' ' << name << ' '
                      << level << std::endl;
        }

        /**
         * @brief Serialize manifest segment record into stream.
         *        Format: <operation-type><whitespace><segment-name><whitespace><level-index>
         *                |int            |char       |string       |char       |int
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

    /**
     * @class level_record_t
     * @brief Represents an operation applicable to level
     *
     */
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
            outStream << static_cast<std::int32_t>(type) << ' ' << static_cast<std::int32_t>(op) << ' ' << level;
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

    using record_t = std::variant<segment_record_t, level_record_t>;
    using storage_t = std::vector<record_t>;

    /**
     * @brief Construct a new manifest t object
     *
     * @param config
     */
    explicit manifest_t(config::shared_ptr_t config);

    /**
     * @brief
     *
     * @return true
     * @return false
     */
    auto open() -> bool;

    /**
     * @brief
     *
     * @return fs::path_t
     */
    auto path() -> fs::path_t;

    /**
     * @brief
     *
     * @param info
     */
    void add(record_t info);

    /**
     * @brief
     *
     * @return true
     * @return false
     */
    auto recover() -> bool;

    /**
     * @brief
     *
     * @return fs::path_t
     */
    auto path() const noexcept -> fs::path_t;

    /**
     * @brief
     *
     * @return std::vector<record_t>
     */
    auto records() const noexcept -> storage_t;

    /**
     * @brief
     *
     */
    void enable();

    /**
     * @brief
     *
     */
    void disable();

  private:
    config::shared_ptr_t   m_config;
    std::string            m_name;
    fs::path_t             m_path;
    storage_t              m_records;
    fs::append_only_file_t m_log;
    bool                   m_enabled{false};
};

using shared_ptr_t = std::shared_ptr<manifest_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<manifest_t>(std::forward<Args>(args)...);
}

} // namespace db::manifest
