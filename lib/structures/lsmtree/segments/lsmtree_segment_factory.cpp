//
// Created by nikon on 2/6/22.
//

#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/lsmtree_mock_segment.h>
#include <structures/lsmtree/segments/lsmtree_regular_segment.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>

namespace structures::lsmtree::segments::factories
{

interface::shared_ptr_t lsmtree_segment_factory(const lsmtree_segment_type_t type,
                                                types::name_t name,
                                                types::path_t path,
                                                memtable_unique_ptr_t pMemtable)
{
    switch (type)
    {
    case lsmtree_segment_type_t::mock_k:
        return mock_segment::make_shared(std::move(path), std::move(pMemtable));
    case lsmtree_segment_type_t::regular_k:
        return regular_segment::make_shared(std::move(path), std::move(name), std::move(pMemtable));
    default:
        assert(false);
        return nullptr;
    }
}

} // namespace structures::lsmtree::segments::factories
