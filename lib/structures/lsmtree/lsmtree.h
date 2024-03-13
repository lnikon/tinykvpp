#ifndef STRUCTURES_LSMTREE_LSMTREE_T_H
#define STRUCTURES_LSMTREE_LSMTREE_T_H

#include <structures/lsmtree/levels/levels.h>
#include <structures/lsmtree/lsmtree_config.h>
#include <structures/lsmtree/lsmtree_types.h>

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
    explicit lsmtree_t(const config::shared_ptr_t pConfig) noexcept;
    lsmtree_t() = delete;
    lsmtree_t(const lsmtree_t &) = delete;
    lsmtree_t &operator=(const lsmtree_t &) = delete;
    lsmtree_t(lsmtree_t &&) = delete;
    lsmtree_t &operator=(lsmtree_t &&) = delete;

    void put(const key_t &key, const value_t &value) noexcept;
    [[nodiscard]] std::optional<record_t> get(const key_t &key) const noexcept;

   private:
    const config::shared_ptr_t m_pConfig;
    memtable::unique_ptr_t m_pTable;
    levels::levels_t m_levels;
    // TODO: bloom_filter_t m_bloom;
};

}  // namespace structures::lsmtree

#endif  // STRUCTURES_LSMTREE_LSMTREE_T_H
