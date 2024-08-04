#ifndef STRUCTURES_LSMTREE_LSMTREE_T_H
#define STRUCTURES_LSMTREE_LSMTREE_T_H

#include <db/manifest/manifest.h>
#include "db/wal/wal.h"
#include <structures/lsmtree/levels/levels.h>
#include <structures/lsmtree/lsmtree_config.h>
#include <structures/lsmtree/lsmtree_types.h>

#include <cassert>
#include <optional>

namespace structures::lsmtree
{

/**
 * @class lsmtree_t
 * @brief
 *
 */
class lsmtree_t
{
  public:
    /**
     * @brief
     *
     * @param pConfig
     */
    explicit lsmtree_t(config::shared_ptr_t pConfig,
                       db::manifest::shared_ptr_t manifest,
                       db::wal::shared_ptr_t wal) noexcept;

    /**
     * @brief
     */
    lsmtree_t() = delete;

    /**
     * @brief
     */
    lsmtree_t(const lsmtree_t &) = delete;

    /**
     * @brief
     *
     * @return
     */
    auto operator=(const lsmtree_t &) -> lsmtree_t & = delete;

    /**
     * @brief
     */
    lsmtree_t(lsmtree_t &&) = delete;

    /**
     * @brief
     *
     * @return
     */
    auto operator=(lsmtree_t &&) -> lsmtree_t & = delete;

    /**
     * @brief
     *
     * @param key
     * @param value
     */
    void put(const key_t &key, const value_t &value) noexcept;

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] auto get(const key_t &key) noexcept -> std::optional<record_t>;

    /**
     * @brief
     *
     * @return
     */
    auto recover() noexcept -> bool;

  private:
    auto restore_manifest() noexcept -> bool;
    auto restore_wal() noexcept -> bool;

    const config::shared_ptr_t m_pConfig;
    std::optional<memtable::memtable_t> m_table;
    db::manifest::shared_ptr_t m_manifest;
    db::wal::shared_ptr_t m_wal;
    levels::levels_t m_levels;
    // TODO: bloom_filter_t m_bloom;
};

} // namespace structures::lsmtree

#endif // STRUCTURES_LSMTREE_LSMTREE_T_H
