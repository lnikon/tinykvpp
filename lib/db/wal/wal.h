#pragma once

#include <cstdint>
#include <filesystem>
#include <fs/append_only_file.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <structures/memtable/memtable.h>
#include <vector>

namespace db::wal
{

class wal_t
{
  public:
    using kv_t = structures::memtable::memtable_t::record_t;

    enum operation_k
    {
        add_k,
        delete_k,
    };

    struct record_t
    {
        operation_k op;
        kv_t kv;
    };

    explicit wal_t(const fs::path_t path)
        : m_path{path},
          m_log{m_path}
    {
        if (!m_log.is_open())
        {
            // TODO(lnikon): Better way to handle. Without exceptions.
            throw std::runtime_error("unable to open wal " + m_path.string());
        }
    }

    void add(const operation_k op, const kv_t &kv) noexcept
    {
        m_records.push_back(record_t{op, kv});

        std::stringstream ss;
        ss << static_cast<std::int32_t>(op) << ' ';
        kv.write(ss);
        m_log.write(ss.str());

        spdlog::debug("add wal log: {}", ss.str());
    }

    void reset()
    {
        m_log.close();
        fs::stdfs::remove(m_path);
        if (!m_log.open())
        {
            throw std::runtime_error("unable to reset wal " + m_path.string());
        }
        else
        {
            spdlog::info("wal reset is successfull " + m_path.string());
        }
    }

    std::vector<record_t> records() noexcept
    {
        return m_records;
    }

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