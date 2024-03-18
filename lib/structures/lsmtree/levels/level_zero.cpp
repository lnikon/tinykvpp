#include "level_zero.h"

#include <structures/lsmtree/segments/helpers.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>

namespace structures::lsmtree::level_zero
{

namespace factories = segments::factories;
namespace helpers = segments::helpers;

level_zero_t::level_zero_t(const config::shared_ptr_t pConfig) noexcept
    : m_pConfig{pConfig},
      m_pStorage{segments::storage::make_shared()}
{
}

segments::interface::shared_ptr_t level_zero_t::compact() const noexcept
{
    // TODO: Use 'level_zero_compactation_strategy' and
    // 'level_zero_compactation_threshold'

    // Number of segments on which level0 should compact itself
    if (m_pStorage->size() <=
        m_pConfig->LSMTreeConfig.LevelZeroCompactionSegmentCount)
    {
        return nullptr;
    }

    // Memtable containing the result of compactation of all segments
    auto pMemtable{memtable::make_unique()};

    // Restore each segment, then merge it with resulting memtable
    for (auto begin{m_pStorage->rbegin()}, end{m_pStorage->rend()};
         begin != end;
         ++begin)
    {
        begin->get()->restore();
        pMemtable->merge(begin->get()->memtable());
    }

    // Create a new segment from a compacted segment
    auto name{helpers::segment_name() + segments::types::name_t{"_compacted"}};
    auto path{helpers::segment_path(m_pConfig->datadir_path(), name)};
    return factories::lsmtree_segment_factory(
        m_pConfig->LSMTreeConfig.SegmentType,
        std::move(name),
        std::move(path),
        std::move(pMemtable));
}

segments::interface::shared_ptr_t level_zero_t::segment(
    const lsmtree_segment_type_t type,
    memtable_unique_ptr_t pMemtable)
{
    // Create a new level0 segment
    const auto name{helpers::segment_name()};
    const auto path{helpers::segment_path(m_pConfig->datadir_path(), name)};
    auto pSegment{factories::lsmtree_segment_factory(
        type, name, path, std::move(pMemtable))};

    // Store newly created segment into the storage
    m_pStorage->emplace(pSegment,
                        segments::storage::last_write_time_comparator_t{});

    // Flush newly created segment
    pSegment->flush();

    // Try to compact the storage
    auto levelZeroCompactedSegment = compact();

    // If the level0 is successfully compacted,
    // then flush the newly compacted segment into the disk,
    // and remove old segments from memory and disk.
    if (levelZeroCompactedSegment)
    {
        // If compactation succeeded, then flush the compacted segment into disk
        levelZeroCompactedSegment->flush();

        // Go over the old segments and remove them from disk
        for (auto begin{m_pStorage->begin()}, end{m_pStorage->end()};
             begin != end;
             ++begin)
        {
            if (std::filesystem::exists(begin->get()->get_path()))
            {
                std::cout << "[level_zero_t::segment]: "
                          << "Removing old segment \""
                          << begin->get()->get_path() << "\"" << std::endl;
                std::filesystem::remove(begin->get()->get_path());
            }
        }

        // Clear the in-memory segments storage
        m_pStorage->clear();

        // Add the new segment into the storage
        m_pStorage->emplace(levelZeroCompactedSegment,
                            segments::storage::last_write_time_comparator_t{});

        // Compacted segment is the new result
        return levelZeroCompactedSegment;
    }

    return pSegment;
}
std::optional<record_t> level_zero_t::record(const key_t& key) const noexcept
{
    std::vector<std::optional<record_t>> result{};
    for (auto begin{m_pStorage->cbegin()}, end{m_pStorage->cend()};
         begin != end;
         ++begin)
    {
        // TODO: Return latest record by timestamp
        result = begin->get()->record(key);
        if (!result.empty())
        {
            return result[0];
        }
    }
    return std::nullopt;
}

}  // namespace structures::lsmtree::level_zero
