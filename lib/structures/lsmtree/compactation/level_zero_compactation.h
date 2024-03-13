#ifndef STRUCTURES_LSMTREE_COMPACTATION_LEVEL_ZERO_COMPACTATION_H
#define STRUCTURES_LSMTREE_COMPACTATION_LEVEL_ZERO_COMPACTATION_H

#include "structures/lsmtree/levels/level_zero.h"

namespace structures::lsmtree::level_zero_compactation
{

class level_zero_compactation_t
{
    explicit level_zero_compactation_t(structures::level_zero::sptr) noexcept;
};

}  // namespace structures::lsmtree::level_zero_compactation

#endif  // STRUCTURES_LSMTREE_COMPACTATION_LEVEL_ZERO_COMPACTATION_H
