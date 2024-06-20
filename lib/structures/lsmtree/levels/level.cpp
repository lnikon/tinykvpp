#include "level.h"
#include "structures/lsmtree/lsmtree_types.h"
#include "structures/lsmtree/segments/lsmtree_regular_segment.h"
#include "structures/lsmtree/segments/segment_storage.h"
#include "structures/lsmtree/segments/helpers.h"
#include "structures/lsmtree/segments/lsmtree_segment_factory.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <ranges>
#include <spdlog/spdlog.h>

namespace structures::lsmtree::level
{

namespace factories = segments::factories;
namespace helpers = segments::helpers;

level_t::level_t(const level_index_type_t levelIndex, const config::shared_ptr_t pConfig) noexcept
    : m_pConfig{pConfig},
      m_levelIndex{levelIndex},
      m_pStorage{segments::storage::make_shared()}
{
}

void level_t::emplace(lsmtree::segments::interface::shared_ptr_t pSegment) noexcept
{
    assert(pSegment);
    spdlog::info("Adding segment {} into level {}", pSegment->get_name(), index());
    if (index() == 0)
    {
        m_pStorage->emplace(pSegment, segments::storage::last_write_time_comparator_t{});
    }
    else
    {
        m_pStorage->emplace(pSegment, segments::storage::key_range_comparator_t{});
    }
}

segments::interface::shared_ptr_t level_t::segment(const lsmtree_segment_type_t type, memtable::memtable_t pMemtable)
{
    // Generate name for the segment
    const auto name{helpers::segment_name()};

    // Generate a path for the segment, including its name, then based on @type and @pMemtable create a segment
    auto pSegment{factories::lsmtree_segment_factory(
        type, name, helpers::segment_path(m_pConfig->datadir_path(), name), std::move(pMemtable))};

    // Store newly created segment into the storage
    emplace(pSegment);

    // Flush newly created segment
    pSegment->flush();

    return pSegment;
}

std::optional<record_t> level_t::record(const key_t &key) const noexcept
{
    std::vector<std::optional<record_t>> result{};
    for (auto begin{m_pStorage->cbegin()}, end{m_pStorage->cend()}; begin != end; ++begin)
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

segments::interface::shared_ptr_t level_t::compact() const noexcept
{
    // If level size hasn't reached the size limit then skip the compactation
    if (m_pStorage->size() <= m_pConfig->LSMTreeConfig.LevelZeroCompactionSegmentCount)
    {
        return nullptr;
    }

    // Memtable containing the result of compactation of all segments
    auto memtable{memtable::memtable_t{}};

    // Restore each segment, then merge it with resulting memtable
    for (auto begin{m_pStorage->rbegin()}, end{m_pStorage->rend()}; begin != end; ++begin)
    {
        begin->get()->restore();
        memtable.merge(begin->get()->memtable().value());
    }

    // Create a new segment from the compacted segment
    auto name{helpers::segment_name() + segments::types::name_t{"_compacted"}};
    return factories::lsmtree_segment_factory(m_pConfig->LSMTreeConfig.SegmentType,
                                              std::move(name),
                                              helpers::segment_path(m_pConfig->datadir_path(), name),
                                              std::move(memtable));
}

segments::interface::shared_ptr_t level_t::merge(segments::interface::shared_ptr_t pSegment) noexcept
{
    // Input memtable to merge with
    auto inMemtableView = pSegment->memtable().value() | std::views::all;
    std::cout << "printing inMemtableView\n";
    for (auto rec : inMemtableView)
    {
        rec.write(std::cout);
    }
    std::cout << std::endl;

    // Segments overlapping with input memtable
    auto overlappingSegmentsView = *m_pStorage | std::views::filter(
                                                     [](auto pSegment) {
                                                         return pSegment->min().value() > pSegment->min().value() ||
                                                                pSegment->max().value() < pSegment->max().value();
                                                     });

    auto memtablesJoined = overlappingSegmentsView |
                           std::views::transform([](auto pSegment) { return pSegment->memtable().value(); }) |
                           std::views::join;

    std::cout << "printing memtablesJoined\n";
    for (auto rec : memtablesJoined)
    {
        rec.write(std::cout);
    }
    std::cout << std::endl;

    // Merge overlapping memtables and segments
    std::vector<memtable::memtable_t::record_t> mergedMemtable;
    std::ranges::merge(inMemtableView, memtablesJoined, std::back_inserter(mergedMemtable), std::less<>{});

    // TODO(lnikon): Make this parameter configurable. Use measurement units(mb).
    const std::size_t segmentSize{1024};
    memtable::memtable_t newMemtable;
    segments::storage::segment_storage_t newSegments;

    std::cout << "merged memtable size " << mergedMemtable.size() << std::endl;
    for (const auto &record : mergedMemtable)
    {
        newMemtable.emplace(record);
        if (newMemtable.size() >= segmentSize)
        {
            auto name = helpers::segment_name();
            newSegments.emplace(
                factories::lsmtree_segment_factory(m_pConfig->LSMTreeConfig.SegmentType,
                                                   name,
                                                   helpers::segment_path(m_pConfig->datadir_path(), name),
                                                   std::move(newMemtable)),
                // TODO: Use min/max comparator
                segments::storage::key_range_comparator_t{});
        }
    }

    // Flush leftover records
    if (!newMemtable.empty())
    {
        std::cout << "flush leftovers size " << newMemtable.size() << std::endl;
        auto name = helpers::segment_name();
        newSegments.emplace(factories::lsmtree_segment_factory(m_pConfig->LSMTreeConfig.SegmentType,
                                                               name,
                                                               helpers::segment_path(m_pConfig->datadir_path(), name),
                                                               std::move(newMemtable)),
                            segments::storage::key_range_comparator_t{});
    }

    // Delete overlapping segments after the merging process is complete
    std::ranges::for_each(overlappingSegmentsView, [this](auto pSegment) { this->purge(pSegment); });

    // Flush new segments
    std::cout << "# of new segments " << newSegments.size() << std::endl;
    for (const auto &pNewCurrentLevelSegment : newSegments)
    {
        std::cout << "flushing merge result " << pNewCurrentLevelSegment->get_path() << std::endl;
        pNewCurrentLevelSegment->flush();
        emplace(pNewCurrentLevelSegment);
    }

    return nullptr;
}

void level_t::purge() const noexcept
{
    assert(m_pStorage);
    spdlog::info("Purging level {} with {} segments", index(), m_pStorage->size());
    purge(*m_pStorage);
}

void level_t::purge(segments::storage::segment_storage_t &storage) const noexcept
{
    // Go over the old segments and remove them from disk
    std::vector<segments::interface::shared_ptr_t> segments;
    for (auto segment : storage)
    {
        segments.push_back(segment);
    }

    for (auto segment : segments)
    {
        assert(segment);
        purge(segment);
    }

    // Clear the in-memory segments storage
    spdlog::info("Clear the in-memory segments storage");
    storage.clear();
}

void level_t::purge(segments::interface::shared_ptr_t pSegment) const noexcept
{
    assert(pSegment);

    spdlog::info("Removing old segment {} from level {}", pSegment->get_name(), index());
    if (std::filesystem::exists(pSegment->get_path()))
    {
        std::filesystem::remove(pSegment->get_path());
    }

    m_pStorage->remove(pSegment);
}

level_t::level_index_type_t level_t::index() const noexcept
{
    return m_levelIndex;
}

} // namespace structures::lsmtree::level
