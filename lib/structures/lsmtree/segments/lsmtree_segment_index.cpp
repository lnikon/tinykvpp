#include <structures/lsmtree/segments/lsmtree_segment_index.h>

namespace structures::lsmtree
{

lsmtree_segment_index_t::offset_type_t lsmtree_segment_index_t::write(
    const key_t &key)
{
    return m_lastOffset;
}

lsmtree_segment_index_t::offset_type_t lsmtree_segment_index_t::read(
    const key_t &key) const
{
    return m_lastOffset;
}

}  // namespace structures::lsmtree
