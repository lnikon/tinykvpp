#include <cassert>

#include <spdlog/spdlog.h>

#include "structures/lsmtree/segments/lsmtree_regular_segment.h"
#include "structures/lsmtree/segments/segment_storage.h"

namespace structures::lsmtree::segments::storage
{

void segment_storage_t::emplace(regular_segment::shared_ptr_t pSegment, segment_comp_t comp)
{
    assert(pSegment);
    assert(m_segmentsMap.size() == m_segmentsVector.size());

    absl::MutexLock lock{&m_mutex};
    if (auto it = m_segmentsMap.find(pSegment->get_name()); it == m_segmentsMap.end())
    {
        m_segmentsMap.emplace(pSegment->get_name(), pSegment);
        m_segmentsVector.emplace(pSegment, comp);
    }

    assert(m_segmentsMap.size() == m_segmentsVector.size());
}

auto segment_storage_t::size() const noexcept -> segment_storage_t::size_type
{
    return std::size(m_segmentsVector);
}

auto segment_storage_t::begin() noexcept -> segment_storage_t::iterator
{
    return std::begin(m_segmentsVector);
}

auto segment_storage_t::end() noexcept -> segment_storage_t::iterator
{
    return std::end(m_segmentsVector);
}

auto segment_storage_t::begin() const noexcept -> segment_storage_t::const_iterator
{
    return std::cbegin(m_segmentsVector);
}

auto segment_storage_t::end() const noexcept -> segment_storage_t::const_iterator
{
    return std::cend(m_segmentsVector);
}

auto segment_storage_t::cbegin() const noexcept -> segment_storage_t::const_iterator
{
    return m_segmentsVector.cbegin();
}

auto segment_storage_t::cend() const noexcept -> segment_storage_t::const_iterator
{
    return m_segmentsVector.cend();
}

auto segment_storage_t::rbegin() noexcept -> segment_storage_t::reverse_iterator
{
    return m_segmentsVector.rbegin();
}

auto segment_storage_t::rend() noexcept -> segment_storage_t::reverse_iterator
{
    return m_segmentsVector.rend();
}

void segment_storage_t::clear() noexcept
{
    absl::WriterMutexLock lock{&m_mutex};
    m_segmentsMap.clear();
    m_segmentsVector.clear();
}

void segment_storage_t::remove(regular_segment::shared_ptr_t pSegment)
{
    absl::WriterMutexLock lock{&m_mutex};

    const auto oldSize = m_segmentsVector.size();
    m_segmentsVector.erase(
        std::remove(std::begin(m_segmentsVector), std::end(m_segmentsVector), pSegment),
        std::end(m_segmentsVector)
    );
    m_segmentsMap.erase(pSegment->get_name());
    const auto newSize = m_segmentsVector.size();

    spdlog::debug(
        "({}): Removed {}, old size {}, new size {}",
        "segment_storage_t::remove",
        pSegment->get_name(),
        oldSize,
        newSize
    );
}

auto segment_storage_t::find(const std::string &name) const noexcept
    -> regular_segment::shared_ptr_t
{
    auto it{m_segmentsMap.find(name)};
    return it != m_segmentsMap.end() ? it->second : nullptr;
}

} // namespace structures::lsmtree::segments::storage

// namespace structures::lsmtree::segments::storage
