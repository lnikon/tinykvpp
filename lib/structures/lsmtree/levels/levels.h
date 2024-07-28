#pragma once

#include "db/manifest/manifest.h"
#include <structures/lsmtree/levels/level.h>
#include <config/config.h>

#include <optional>

namespace structures::lsmtree::levels
{

/**
 * @class levels_t
 * @brief
 *
 */
class levels_t
{
  public:
    using levels_storage_t = std::vector<structures::lsmtree::level::shared_ptr_t>;

    /**
     * @brief
     *
     * @param pConfig
     */
    explicit levels_t(const config::shared_ptr_t pConfig, db::manifest::shared_ptr_t manifest) noexcept;

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
    [[maybe_unused]] segments::regular_segment::shared_ptr_t segment(const lsmtree_segment_type_t type,
                                                                     memtable::memtable_t memtable);
    /**
     * @brief Appends an additional level to the levels storage.
     *        If the table has #L levels, this new level will act as an #L+1th level.
     *
     * @return owning pointer to the newly created level
     */
    [[maybe_unused]] level::shared_ptr_t level() noexcept;

    /**
     * @brief
     *
     * @param idx
     */
    [[maybe_unused]] level::shared_ptr_t level(const std::size_t idx) noexcept;

    levels_storage_t::size_type size() const noexcept;

  private:
    const config::shared_ptr_t m_pConfig;
    levels_storage_t m_levels;
    db::manifest::shared_ptr_t m_manifest;
};

} // namespace structures::lsmtree::levels
