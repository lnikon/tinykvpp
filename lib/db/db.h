#pragma once

#include "structures/memtable/memtable.h"
#include "wal/wal.h"
#include "config/config.h"
#include "structures/lsmtree/lsmtree.h"
#include "db/manifest/manifest.h"

namespace db
{

class db_t
{
  public:
    explicit db_t(config::shared_ptr_t config, wal::shared_ptr_t wal);

    db_t(db_t &&other) noexcept
        : m_config{std::move(other.m_config)},
          m_pManifest{std::move(other.m_pManifest)},
          m_pWal{std::move(other.m_pWal)},
          m_lsmTree{std::move(other.m_lsmTree)}
    {
    }

    auto operator=(db_t &&other) noexcept -> db_t &
    {
        if (this != &other)
        {
            db_t temp{std::move(other)};
            swap(temp);
        }

        return *this;
    }

    db_t(const db_t &) = delete;
    auto operator=(const db_t &) -> db_t & = delete;

    ~db_t() noexcept = default;

    /**
     * @brief Open database
     *
     * @return true
     * @return false
     */
    [[nodiscard]] auto open() -> bool;

    /**
     * @brief Put key-value pair into the database
     *
     * @param key
     * @param value
     */
    [[nodiscard]] auto put(const structures::lsmtree::key_t   &key,
                           const structures::lsmtree::value_t &value) noexcept -> bool;

    /**
     * @brief Get key-value pair from database
     *
     * @param key
     * @return std::optional<structures::memtable::memtable_t::record_t>
     */
    auto get(const structures::lsmtree::key_t &key)
        -> std::optional<structures::memtable::memtable_t::record_t>;

    /**
     *
     * @return Return configuration passed to the database during the start
     */
    auto config() const noexcept -> config::shared_ptr_t;

  private:
    auto prepare_directory_structure() -> bool;

    void swap(db_t &other) noexcept;

    config::shared_ptr_t           m_config;
    manifest::shared_ptr_t         m_pManifest;
    wal::shared_ptr_t              m_pWal;
    structures::lsmtree::lsmtree_t m_lsmTree;
};

using shared_ptr_t = std::shared_ptr<db_t>;

template <typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<db_t>(std::forward<Args>(args)...);
}

class db_builder_t
{
  public:
    [[nodiscard]] auto build(config::shared_ptr_t config, wal::shared_ptr_t wal)
        -> std::optional<db_t>
    {
        return std::make_optional(db_t{std::move(config), std::move(wal)});
    }
};
} // namespace db
