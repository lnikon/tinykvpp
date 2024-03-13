#include <structures/lsmtree/segments/segment_storage.h>

#include <cassert>

namespace structures::lsmtree::segments::storage
{

void segment_storage_t::emplace(segment::shared_ptr_t pSegment,
                                segment_comp_t comp)
{
    assert(pSegment);

    std::lock_guard lg(m_mutex);
    if (auto it = m_segmentsMap.find(pSegment->get_name());
        it == m_segmentsMap.end())
    {
        m_segmentsMap.emplace(pSegment->get_name(), pSegment);
        m_segmentsVector.emplace(pSegment, comp);
    }
}

// TODO: Implement remove
// void segment_storage_t::remove(const name_t &name)
//{
//    assert(!name.empty());
//
//    std::lock_guard lg(m_mutex);
//    if (auto it = m_segmentsMap.find(name); it == m_segmentsMap.end())
//    {
//        m_segmentsMap.erase(name);
//        m_segmentsVector.erase(
//            std::remove_if(m_segmentsVector.begin(),
//                           m_segmentsVector.end(),
//                           [&name](auto pSegment)
//                           { return pSegment->get_name() == name; }),
//            m_segmentsVector.end());
//    }
//}

segment_storage_t::storage_t::size_type segment_storage_t::size() const noexcept
{
    return std::size(m_segmentsVector);
}

segment_storage_t::iterator segment_storage_t::begin() noexcept
{
    return std::begin(m_segmentsVector);
}

segment_storage_t::iterator segment_storage_t::end() noexcept
{
    return std::end(m_segmentsVector);
}

segment_storage_t::const_iterator segment_storage_t::cbegin() const noexcept
{
    return m_segmentsVector.cbegin();
}

segment_storage_t::const_iterator segment_storage_t::cend() const noexcept
{
    return m_segmentsVector.cend();
}

void segment_storage_t::clear() noexcept
{
    m_segmentsMap.clear();
    m_segmentsVector.clear();
}

}  // namespace structures::lsmtree::segments::storage

// namespace structures::lsmtree::segments::storage
