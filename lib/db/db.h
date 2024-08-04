#pragma once

#include "structures/memtable/memtable.h"
#include "wal/wal.h"
#include <config/config.h>
#include <db/db_config.h>
#include <structures/lsmtree/lsmtree.h>
#include <db/manifest/manifest.h>
#include <fs/append_only_file.h>

namespace db
{

class db_t
{
  public:
    /**
     * @brief Construct a new db_t object
     *
     * @param config
     */
    explicit db_t(config::shared_ptr_t config);

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
    void put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value);

    /**
     * @brief Get key-value pair from database
     *
     * @param key
     * @return std::optional<structures::memtable::memtable_t::record_t>
     */
    auto get(const structures::lsmtree::key_t &key) -> std::optional<structures::memtable::memtable_t::record_t>;

  private:
    auto prepare_directory_structure() -> bool;

    config::shared_ptr_t m_config;
    manifest::shared_ptr_t m_manifest;
    wal::shared_ptr_t m_wal;
    structures::lsmtree::lsmtree_t m_lsmTree;
};

} // namespace db
