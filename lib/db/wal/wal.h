#pragma once

#include "fs/types.h"
#include <cstdint>
#include <filesystem>
#include <fs/append_only_file.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <structures/memtable/memtable.h>
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

    enum operation_k : int8_t
    {
        undefined_k = -1,
        add_k,
        delete_k,
    };

    struct record_t
    {
        operation_k op;
        kv_t kv;

        template <typename Stream> void write(Stream &stream) const
        {
            stream << static_cast<std::int32_t>(op) << ' ';
            kv.write(stream);
        }

        template <typename Stream> void read(Stream &stream)
        {
            int32_t opInt{0};
            stream >> opInt;
            op = static_cast<operation_k>(opInt);

            kv.read(stream);
        }
    };

    /**
     * @brief Construct a new wal t object
     *
     * @param path
     */
    explicit wal_t(fs::path_t path);

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
     * @param rec
     */
    void add(record_t rec) noexcept;

    /**
     * @brief
     *
     */
    void reset();

    /**
     * @brief
     *
     * @return true
     * @return false
     */
    auto recover() noexcept -> bool;

    /**
     * @brief
     *
     * @return std::vector<record_t>
     */
    auto records() noexcept -> std::vector<record_t>;

  private:
    fs::path_t m_path;
    std::vector<record_t> m_records;
    fs::append_only_file_t m_log;
};

using shared_ptr_t = std::shared_ptr<wal_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<wal_t>(std::forward<Args>(args)...);
}

} // namespace db::wal