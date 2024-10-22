#pragma once

#include "structures/lsmtree/segments/lsmtree_regular_segment.h"
#include <db/manifest/manifest.h>
#include <config/config.h>
#include <structures/lsmtree/segments/segment_storage.h>

#include <absl/synchronization/mutex.h>

namespace structures::lsmtree::level
{

class level_t
{
  public:
    using level_index_type_t = std::size_t;

    /**
     * @brief
     *
     * @param pConfig
     */
    explicit level_t(level_index_type_t         levelIndex,
                     config::shared_ptr_t       pConfig,
                     db::manifest::shared_ptr_t manifest) noexcept;

    /**
     * @brief
     *
     * @param pSegment
     */
    void emplace(const segments::regular_segment::shared_ptr_t &pSegment) noexcept;

    /**
     * @brief Create an immutable segment of a given type for the @pMemtable.
     *        The newly created segments is emplaced into the underlying storage
     *        of the level, and flushed onto the disk.
     *
     * @param type
     * @param pMemtable
     * @return owning pointer to the newly created segment
     */
    [[maybe_unused]] auto segment(memtable::memtable_t pMemtable) -> segments::regular_segment::shared_ptr_t;

    /**
     * @brief Creates a new segment from the given memtable and stores it.
     *
     * This function generates a path for the segment using the provided name,
     * creates a segment based on the memtable, stores the newly created segment
     * into the storage, and then flushes the segment.
     *
     * @param memtable The memtable to be converted into a segment.
     * @param name The name to be used for the segment.
     * @return A shared pointer to the newly created segment.
     */
    auto segment(memtable::memtable_t memtable, const std::string &name) -> segments::regular_segment::shared_ptr_t;

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] auto record(const key_t &key) const noexcept -> std::optional<memtable::memtable_t::record_t>;

    /**
     * @brief Compact level0 into a single segment in 'ReadyToFlush' state
     */
    [[nodiscard]] auto compact() const noexcept -> segments::regular_segment::shared_ptr_t;

    /**
     * @brief
     *
     * @param pSegment
     */
    void merge(const segments::regular_segment::shared_ptr_t &pSegment) noexcept;

    /**
     * @brief Purge segments from memory and disk
     *
     * @return
     */
    void purge() noexcept;

    /**
     * @brief Find a segment by its name and purge it
     *
     * @return
     */
    void purge(const segments::types::name_t &segmentName) noexcept;

    auto restore() noexcept -> void;

    /**
     * @brief Return index of the level.
     *
     * @return An unsigned integer
     */
    [[nodiscard]] auto index() const noexcept -> level_index_type_t;

    [[__nodiscard__]] auto bytes_used() const noexcept -> std::size_t;

  private:
    /**
     * @brief Purges the specified segment from the level.
     *
     * This function removes the given segment from the level, logs the removal,
     * updates the manifest to reflect the removal, purges the segment, and
     * removes it from storage.
     *
     * @param pSegment A shared pointer to the segment to be purged. Must not be null.
     */
    void purge(const segments::regular_segment::shared_ptr_t &pSegment) noexcept;

    mutable absl::Mutex m_mutex;

    const level_index_type_t             m_levelIndex;
    config::shared_ptr_t                 m_pConfig;
    segments::storage::segment_storage_t m_storage;
    db::manifest::shared_ptr_t           m_manifest;
};

using shared_ptr_t = std::shared_ptr<level_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<level_t>(std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::level
