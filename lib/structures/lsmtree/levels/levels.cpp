//
// Created by nikon on 3/8/24.
//

#include "levels.h"

namespace structures::lsmtree::levels
{

levels_t::levels_t(const config::shared_ptr_t pConfig) noexcept
    : m_pConfig{pConfig},
      m_level_zero(m_pConfig)
{
}

segments::interface::shared_ptr_t levels_t::segment(
    const structures::lsmtree::lsmtree_segment_type_t type,
    structures::lsmtree::memtable_unique_ptr_t pMemtable)
{
    assert(pMemtable);
    return m_level_zero.segment(type, std::move(pMemtable));
}
std::optional<record_t> levels_t::record(const key_t& key) const noexcept
{
    // Search for a key in level zero
    auto result{m_level_zero.record(key)};

    // If can't found in level zero, proceed to the rest of the tree
    if (!result.has_value())
    {
        // TODO: result = m_level_non_zero.record(key);
    }

    return result;
}

}  // namespace structures::lsmtree::levels
