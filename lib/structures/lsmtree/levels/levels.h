#pragma once

#include "db/manifest/manifest.h"
#include <structures/lsmtree/levels/level.h>
#include <config/config.h>

#include <optional>
#include <vector>

#include <absl/synchronization/mutex.h>
#include <absl/synchronization/notification.h>

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
    explicit levels_t(config::shared_ptr_t pConfig, db::manifest::shared_ptr_t pManifest) noexcept;

    ~levels_t() noexcept;

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] auto record(const key_t &key) const noexcept -> std::optional<record_t>;

    /**
     * @brief Creates a new segment from the provided memtable and handles compaction across levels.
     *
     * This function performs the following steps:
     * 1. Ensures that level zero exists, creating it if necessary.
     * 2. Creates a new segment for the provided memtable.
     * 3. Updates the manifest with the new segment.
     * 4. Attempts to compact segments across all levels, starting from level zero.
     * 5. If compaction is successful, updates the manifest and flushes the compacted segment to disk.
     * 6. Merges the compacted segment into the next level, creating the next level if it does not exist.
     * 7. Purges the compacted segment and updates the manifest accordingly.
     * 8. Returns the resulting segment from the compaction process, or the newly created segment if no compaction
     * occurred.
     *
     * @param memtable The memtable to be converted into a segment.
     * @return A shared pointer to the resulting segment.
     */
    [[maybe_unused]] auto compact() -> segments::regular_segment::shared_ptr_t;

    /**
     * @brief Creates and returns a shared pointer to a new level.
     *
     * This function creates a new level using the current size of the levels container,
     * the configuration pointer, and the manifest pointer. The new level is added to
     * the levels container and a shared pointer to this new level is returned.
     *
     * @return level::shared_ptr_t A shared pointer to the newly created level.
     */
    [[maybe_unused]] auto level() noexcept -> level::shared_ptr_t;

    /**
     * @brief Retrieves a shared pointer to the level at the specified index.
     *
     * This function returns a shared pointer to the level object located at the
     * given index within the levels container. The index must be within the bounds
     * of the container.
     *
     * @param idx The index of the level to retrieve.
     * @return level::shared_ptr_t A shared pointer to the level at the specified index.
     *
     * @note This function is marked as noexcept and [[maybe_unused]].
     * @throws Assertion failure if idx is out of bounds.
     */
    [[maybe_unused]] auto level(std::size_t idx) noexcept -> level::shared_ptr_t;

    /**
     * @brief Returns the number of levels in the LSM tree.
     *
     * This function provides the size of the levels storage container.
     *
     * @return levels_storage_t::size_type The number of levels.
     */
    [[nodiscard]] auto size() const noexcept -> levels_storage_t::size_type;

    [[nodiscard]] auto
    flush_to_level0(memtable::memtable_t memtable) const noexcept -> segments::regular_segment::shared_ptr_t;

    auto restore() noexcept -> void;

  private:
    config::shared_ptr_t m_pConfig;

    mutable absl::Mutex        m_mutex;
    db::manifest::shared_ptr_t m_pManifest;
    levels_storage_t           m_levels;

    mutable absl::Notification m_level0_segment_flushed_notification;
    std::jthread               m_compaction_thread;
};

} // namespace structures::lsmtree::levels
