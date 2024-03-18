//
// Created by nikon on 3/8/24.
//

#ifndef STRUCTURES_LSMTREE_LEVELS_LEVELS_T_H
#define STRUCTURES_LSMTREE_LEVELS_LEVELS_T_H

#include <config/config.h>
#include <structures/lsmtree/levels/level_non_zero.h>
#include <structures/lsmtree/levels/level_zero.h>

namespace structures::lsmtree::levels
{

class levels_t
{
   public:
    explicit levels_t(const config::shared_ptr_t pConfig) noexcept;

    [[nodiscard]] std::optional<record_t> record(
        const key_t &key) const noexcept;

    [[maybe_unused]] segments::interface::shared_ptr_t segment(
        const lsmtree_segment_type_t type,
        memtable::unique_ptr_t pMemtable);

   private:
    const config::shared_ptr_t m_pConfig;
    level_zero::level_zero_t m_level_zero;

    // TODO: Implement support for non zero levels
//    level_non_zero::level_non_zero_t m_level_non_zero;
};

}  // namespace structures::lsmtree::levels

#endif  // STRUCTURES_LSMTREE_LEVELS_LEVELS_T_H
