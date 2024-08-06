#pragma once

#include "db/manifest/manifest.h"
#include <structures/lsmtree/levels/level.h>
#include <config/config.h>

#include <optional>
#include <vector>

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
    explicit levels_t(config::shared_ptr_t pConfig, db::manifest::shared_ptr_t manifest) noexcept;

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] auto record(const key_t &key) const noexcept -> std::optional<record_t>;

    /**
     * @brief
     *
     * @param type
     * @param pMemtable
     */
    [[maybe_unused]] auto segment(memtable::memtable_t memtable) -> segments::regular_segment::shared_ptr_t;
    /**
     * @brief Appends an additional level to the levels storage.
     *        If the table has #L levels, this new level will act as an #L+1th level.
     *
     * @return owning pointer to the newly created level
     */
    [[maybe_unused]] auto level() noexcept -> level::shared_ptr_t;

    /**
     * @brief
     *
     * @param idx
     */
    [[maybe_unused]] auto level(std::size_t idx) noexcept -> level::shared_ptr_t;

    [[nodiscard]] auto size() const noexcept -> levels_storage_t::size_type;

  private:
    config::shared_ptr_t m_pConfig;
    levels_storage_t m_levels;
    db::manifest::shared_ptr_t m_manifest;
};

} // namespace structures::lsmtree::levels
