#ifndef STRUCTURES_LSMTREE_LSMTREE_T_H
#define STRUCTURES_LSMTREE_LSMTREE_T_H

#include <db/manifest.h>
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
    explicit lsmtree_t(const config::shared_ptr_t pConfig, db::manifest::shared_ptr_t manifest) noexcept;

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
    lsmtree_t &operator=(const lsmtree_t &) = delete;

    /**
     * @brief
     */
    lsmtree_t(lsmtree_t &&) = delete;

    /**
     * @brief
     *
     * @return
     */
    lsmtree_t &operator=(lsmtree_t &&) = delete;

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
    [[nodiscard]] std::optional<record_t> get(const key_t &key) noexcept;

    /**
     * @brief
     *
     * @return
     */
    bool restore() noexcept;

  private:
    const config::shared_ptr_t m_pConfig;
    std::optional<memtable::memtable_t> m_table;
    db::manifest::shared_ptr_t m_manifest;
    levels::levels_t m_levels;
    // TODO: bloom_filter_t m_bloom;
};

} // namespace structures::lsmtree

#endif // STRUCTURES_LSMTREE_LSMTREE_T_H
