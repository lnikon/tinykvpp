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
    explicit db_t(const config::shared_ptr_t config);

    [[nodiscard]] bool open();

    void put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value);
    std::optional<structures::memtable::memtable_t::record_t> get(const structures::lsmtree::key_t &key);

  private:
    bool prepare_directory_structure();

  private:
    const config::shared_ptr_t m_config;
    manifest::shared_ptr_t m_manifest;
    wal::shared_ptr_t m_wal;
    structures::lsmtree::lsmtree_t m_lsmTree;
};

} // namespace db
