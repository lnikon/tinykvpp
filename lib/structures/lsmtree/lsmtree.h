#ifndef STRUCTURES_LSMTREE_LSMTREE_T_H
#define STRUCTURES_LSMTREE_LSMTREE_T_H

#include <structures/lsmtree/levels/levels.h>
#include <structures/lsmtree/lsmtree_config.h>
#include <structures/lsmtree/lsmtree_types.h>

#include <optional>

namespace structures::lsmtree
{

class db_file_t
{
  public:
    explicit db_file_t(const config::shared_ptr_t config);

    [[nodiscard]] bool recover() noexcept;

  private:
};

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
    explicit lsmtree_t(const config::shared_ptr_t pConfig) noexcept;

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
    levels::levels_t m_levels;
    // TODO: bloom_filter_t m_bloom;
};

} // namespace structures::lsmtree

#endif // STRUCTURES_LSMTREE_LSMTREE_T_H
