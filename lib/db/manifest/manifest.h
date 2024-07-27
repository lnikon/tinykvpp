#pragma once

#include <fs/append_only_file.h>

#include <spdlog/spdlog.h>
#include <vector>
#include <variant>

namespace db::manifest
{

/**
 * @class manifest_t
 * @brief Manipulate disk-level manifest file
 */
struct manifest_t
{
    // using level_index_t = structures::lsmtree::level::level_t::level_index_type_t;
    using level_index_t = std::size_t;
    using segment_name_t = std::string;
    using segment_names_t = std::vector<std::string>;
    using storage_t = std::unordered_map<level_index_t, segment_names_t>;

    enum record_type_k
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
        enum class operation_k
        {
            add_segment_k = 0,
            remove_segment_k,
        };

        std::string ToString(operation_k) const
        {
            switch (op)
            {
            case operation_k::add_segment_k:
                return std::string("add_segment_k");
            case operation_k::remove_segment_k:
                return std::string("remove_segment_k");
            default:
                return std::string("unkown op");
            }
        }

        std::string ToString() const
        {
            std::stringstream ss;
            write(ss);
            return ss.str();
        }

        template <typename stream_gt> void write(stream_gt &os) const
        {
            os << static_cast<std::int32_t>(type) << ' ' << static_cast<std::int32_t>(op) << ' ' << name << ' ' << level
               << std::endl;
        }

        /**
         * @brief Serialize manifest segment record into stream.
         *        Format: <operation-type><whitespace><segment-name><whitespace><level-index>
         *                |int            |char       |string       |char       |int
         *
         * @tparam stream_gt
         * @param os
         */
        template <typename stream_gt> void read(stream_gt &os)
        {
            std::int32_t op_int{0};
            os >> op_int;
            op = static_cast<operation_k>(op_int);

            os >> name;

            os >> level;
        }

        const record_type_k type{record_type_k::segment_k};
        operation_k op;
        segment_name_t name;
        level_index_t level;
    };

    /**
     * @class level_record_t
     * @brief Represents an operation applicable to level
     *
     */
    struct level_record_t
    {
        enum class operation_k
        {
            add_level_k,
            compact_level_k,
            purge_level_k,
        };

        std::string ToString(operation_k op) const
        {
            switch (op)
            {
            case operation_k::add_level_k:
                return std::string("add_level_k");
            case operation_k::compact_level_k:
                return std::string("compact_level_k");
            case operation_k::purge_level_k:
                return std::string("purge_level_k");
            default:
                return std::string("unkown op");
            }
        }

        std::string ToString() const
        {
            std::stringstream ss;
            write(ss);
            return ss.str();
        }

        /**
         * @brief Serialize manifest level record into stream.
         *        Format: <operation-type><whitespace><level-index>
         *                |int            |char       |int
         *
         * @tparam stream_gt
         * @param os
         */
        template <typename stream_gt> void write(stream_gt &os) const
        {
            os << static_cast<std::int32_t>(type) << ' ' << static_cast<std::int32_t>(op) << ' ' << level << std::endl;
        }

        template <typename stream_gt> void read(stream_gt &os)
        {
            std::int32_t op_int{0};
            os >> op_int;
            op = static_cast<operation_k>(op_int);

            os >> level;
        }

        const record_type_k type{record_type_k::level_k};
        operation_k op;
        level_index_t level;
    };

    using record_t = std::variant<segment_record_t, level_record_t>;

    explicit manifest_t(const fs::path_t path);

    void add(record_t info)
    {
        if (!m_enabled)
        {
            return;
        }

        m_records.emplace_back(info);

        std::stringstream ss;
        std::visit([&ss](auto &&record) { record.write(ss); }, info);
        m_log.write(ss.str());
    }

    void print() const
    {
        for (const auto &rec : m_records)
        {
            std::visit(
                [](auto &&arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, segment_record_t>)
                    {
                        spdlog::info("{} on with name {} level {}", arg.ToString(arg.op), arg.name, arg.level);
                    }
                    else if constexpr (std::is_same_v<T, level_record_t>)
                    {
                        spdlog::info("{} on level {}", arg.ToString(arg.op), arg.level);
                    }
                },
                rec);
        }
    }

    bool recover()
    {
        spdlog::info("recovering manifest file");
        auto ss = m_log.stream();
        std::int32_t record_type_int{0};
        while (ss >> record_type_int)
        {
            const record_type_k record_type = static_cast<record_type_k>(record_type_int);
            switch (record_type)
            {
            case record_type_k::segment_k:
            {
                segment_record_t record;
                record.read(ss);
                spdlog::info("recovered segment_record={}", record.ToString());
                m_records.emplace_back(record);
                break;
            }
            case record_type_k::level_k:
            {
                level_record_t record;
                record.read(ss);
                spdlog::info("recovered level_record={}", record.ToString());
                m_records.emplace_back(record);
                break;
            }
            default:
            {
                spdlog::error("undhandled record_type_int={}", record_type_int);
                break;
            }
            }
        }

        return true;
    }

    fs::path_t path() const noexcept
    {
        return m_path;
    }

    std::vector<record_t> records() const noexcept
    {
        return m_records;
    }

    void enable()
    {
        m_enabled = true;
    }

    void disable()
    {
        m_enabled = false;
    }

  private:
    void update(const segment_record_t &info)
    {
        switch (info.op)
        {
        case segment_record_t::operation_k::add_segment_k:
            throw std::runtime_error("add_segment_k not implemented");
            break;
        case segment_record_t::operation_k::remove_segment_k:
            throw std::runtime_error("remove_segment_k not implemented");
            break;
        default:
            assert(false);
        }
    }

    void update(const level_record_t &info)
    {
        switch (info.op)
        {
        case level_record_t::operation_k::add_level_k:
            throw std::runtime_error("add_level_k not implemented");
            break;
        case level_record_t::operation_k::compact_level_k:
            throw std::runtime_error("compact_level_k not implemented");
            break;
        default:
            assert(false);
        }
    }

  private:
    const fs::path_t m_path;
    std::vector<record_t> m_records;
    fs::append_only_file_t m_log;
    bool m_enabled{true};
};

using shared_ptr_t = std::shared_ptr<manifest_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<manifest_t>(std::forward<Args>(args)...);
}

} // namespace db::manifest
