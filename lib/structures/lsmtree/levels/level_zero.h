#ifndef STRUCTURES_LSMTREE_LEVEL_ZERO_H
#define STRUCTURES_LSMTREE_LEVEL_ZERO_H

#include <config/config.h>
#include <structures/lsmtree/segments/segment_storage.h>

namespace structures::lsmtree::level_zero
{

class level_zero_t
{
   public:
    explicit level_zero_t(const config::shared_ptr_t pConfig) noexcept;

    void emplace(segments::interface::shared_ptr_t pSegment) noexcept;

    [[maybe_unused]] segments::interface::shared_ptr_t segment(
        const lsmtree_segment_type_t type,
        memtable_unique_ptr_t pMemtable);

    std::optional<record_t> record(const key_t& key) const noexcept;

    //! \brief Compact level0 into a single segment in 'ReadyToFlush' state
    //! \return Compacted segment
    segments::interface::shared_ptr_t compact() const noexcept;
    
   private:
    const config::shared_ptr_t m_pConfig;
    segments::storage::shared_ptr_t m_pStorage;
};

using shared_ptr_t = std::shared_ptr<level_zero_t>;

template <typename... Args>
auto make_shared(Args... args)
{
    return std::make_shared<level_zero_t>(std::forward<Args>(args)...);
}

}  // namespace structures::lsmtree::level_zero

#endif  // STRUCTURES_LSMTREE_LEVEL_ZERO_H
