#pragma once

#include <config/config.h>
#include <structures/lsmtree/segments/segment_storage.h>

namespace structures::lsmtree::level
{

class level_t
{
  public:
    /**
     * @brief
     *
     * @param pConfig
     */
    explicit level_t(const config::shared_ptr_t pConfig) noexcept;

    /**
     * @brief
     *
     * @param pSegment
     */
    void emplace(lsmtree::segments::interface::shared_ptr_t pSegment) noexcept;

    /**
     * @brief
     *
     * @param type
     * @param pMemtable
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

  private:
    void purge(segments::storage::segment_storage_t& m_pStorage) const noexcept;
    void purge(segments::interface::shared_ptr_t pSegment) const noexcept;

  private:
    const config::shared_ptr_t m_pConfig;
    segments::storage::shared_ptr_t m_pStorage;
};

using shared_ptr_t = std::shared_ptr<level_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<level_t>(std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::level
