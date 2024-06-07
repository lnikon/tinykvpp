#pragma once

#include <optional>
#include <structures/lsmtree/levels/level.h>
#include <config/config.h>

namespace structures::lsmtree::levels
{

class levels_t
{
  public:
    using levels_storage_t = std::vector<level::shared_ptr_t>;

    /**
     * @brief
     *
     * @param pConfig
     */
    explicit levels_t(const config::shared_ptr_t pConfig) noexcept;

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] std::optional<record_t> record(const key_t &key) const noexcept;

    /**
     * @brief
     *
     * @param type
     * @param pMemtable
     */
    [[maybe_unused]] segments::interface::shared_ptr_t segment(const lsmtree_segment_type_t type,
                                                               memtable::memtable_t memtable);
    /**
     * @brief
     *
     * @return
     */
    [[maybe_unused]] level::shared_ptr_t level() noexcept;

   /**
    * @brief 
    *
    * @param idx 
    */
   [[maybe_unused]] level::shared_ptr_t level(const std::size_t idx) noexcept;

  private:
    const config::shared_ptr_t m_pConfig;
    levels_storage_t m_levels;
};

} // namespace structures::lsmtree::levels
