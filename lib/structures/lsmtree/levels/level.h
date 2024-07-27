#pragma once

#include <db/manifest.h>
#include <config/config.h>
#include <structures/lsmtree/segments/segment_storage.h>

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
    explicit level_t(const level_index_type_t levelIndex,
                     const config::shared_ptr_t pConfig,
                     db::manifest::shared_ptr_t manifest) noexcept;

    /**
     * @brief
     *
     * @param pSegment
     */
    void emplace(lsmtree::segments::interface::shared_ptr_t pSegment) noexcept;

    /**
     * @brief Create an immutable segment of a given type for the @pMemtable.
     *        The newly created segments is emplaced into the underlying storage
     *        of the level, and fluhed onto the disk.
     *
     * @param type
     * @param pMemtable
     * @return owning pointer to the newly created segment
     */
    [[maybe_unused]] segments::interface::shared_ptr_t segment(const lsmtree_segment_type_t type,
                                                               memtable::memtable_t pMemtable);

    /**
     * @brief
     *
     * @param key
     */
    [[nodiscard]] std::optional<record_t> record(const key_t &key) const noexcept;

    /**
     * @brief Compact level0 into a single segment in 'ReadyToFlush' state
     */
    segments::interface::shared_ptr_t compact() const noexcept;

    /**
     * @brief
     *
     * @param pSegment
     */
    segments::interface::shared_ptr_t merge(segments::interface::shared_ptr_t pSegment) noexcept;

    /**
     * @brief Purge segments from memory and disk
     *
     * @return
     */
    void purge() const noexcept;

    /**
     * @brief Find a segment by its name and purge it
     *
     * @return
     */
    void purge(const segments::types::name_t &segment_name) const noexcept;

    segments::storage::shared_ptr_t storage()
    {
        return m_pStorage;
    }

    /**
     * @brief Return index of the level.
     *
     * @return An unsigned integer
     */
    level_index_type_t index() const noexcept;

  private:
    void purge(segments::storage::segment_storage_t &m_pStorage) const noexcept;
    void purge(segments::interface::shared_ptr_t pSegment) const noexcept;

  private:
    const config::shared_ptr_t m_pConfig;
    const level_index_type_t m_levelIndex;
    segments::storage::shared_ptr_t m_pStorage;
    db::manifest::shared_ptr_t m_manifest;
};

using shared_ptr_t = std::shared_ptr<level_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<level_t>(std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::level
