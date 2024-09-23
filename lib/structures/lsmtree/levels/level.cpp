#include "level.h"
#include "structures/lsmtree/lsmtree_types.h"
#include "structures/lsmtree/segments/lsmtree_segment_factory.h"
#include "structures/lsmtree/segments/segment_storage.h"
#include "structures/lsmtree/segments/helpers.h"
#include "structures/memtable/memtable.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <queue>
#include <ranges>
#include <utility>
#include <cmath>

#include <spdlog/spdlog.h>

namespace structures::lsmtree::level
{

namespace helpers = segments::helpers;

using segment_operation_k = db::manifest::manifest_t::segment_record_t::operation_k;

level_t::level_t(const level_index_type_t levelIndex,
                 config::shared_ptr_t pConfig,
                 db::manifest::shared_ptr_t manifest) noexcept
    : m_pConfig{std::move(pConfig)},
      m_levelIndex{levelIndex},
      m_pStorage{segments::storage::make_shared()},
      m_manifest{std::move(std::move(manifest))}
{
}

void level_t::emplace(const lsmtree::segments::regular_segment::shared_ptr_t &pSegment) noexcept
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

    m_manifest->add(db::manifest::manifest_t::segment_record_t{
        .op = segment_operation_k::add_segment_k, .name = pSegment->get_name(), .level = index()});
}

auto level_t::segment(memtable::memtable_t memtable) -> segments::regular_segment::shared_ptr_t
{
    // Generate name for the segment
    const auto name{fmt::format("{}_{}", helpers::segment_name(), index())};

    // Generate a path for the segment, including its name, then based on @type and @pMemtable create a segment
    auto pSegment{segments::factories::lsmtree_segment_factory(
        name, helpers::segment_path(m_pConfig->datadir_path(), name), std::move(memtable))};

    // Store newly created segment into the storage
    emplace(pSegment);

    // Flush newly created segment
    pSegment->flush();

    return pSegment;
}

auto level_t::record(const key_t &key) const noexcept -> std::optional<memtable::memtable_t::record_t>
{
    for (auto begin{m_pStorage->cbegin()}, end{m_pStorage->cend()}; begin != end; ++begin)
    {
        // TODO: Return latest record by timestamp
        const auto &result = begin->get()->record(key);
        if (!result.empty())
        {
            spdlog::info("Found record {} at level {} in segment {}", key.m_key, index(), begin->get()->get_name());
            return result[0];
        }
    }
    return std::nullopt;
}

template <typename T, typename U = T> struct IteratorCompare
{
    auto operator()(const std::pair<typename T::const_iterator, typename T::const_iterator> &lhs,
                    const std::pair<typename U::const_iterator, typename U::const_iterator> &rhs) const -> bool
    {
        return *lhs.first > *rhs.first;
    }
};

auto level_t::compact() const noexcept -> segments::regular_segment::shared_ptr_t
{
    std::size_t num_of_bytes_used{0};
    for (auto begin{m_pStorage->begin()}; begin != m_pStorage->end(); ++begin)
    {
        num_of_bytes_used += (*begin)->num_of_bytes_used();
    }

    // If level size hasn't reached the size limit then skip the compaction
    if ((index() == 0 && num_of_bytes_used >= m_pConfig->LSMTreeConfig.LevelZeroCompactionThreshold) ||
        (index() != 0 &&
         num_of_bytes_used >= m_pConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold * std::pow(10, index())))
    {
        std::priority_queue<
            std::pair<typename memtable::memtable_t::const_iterator, typename memtable::memtable_t::const_iterator>,
            std::vector<std::pair<typename memtable::memtable_t::const_iterator,
                                  typename memtable::memtable_t::const_iterator>>,
            IteratorCompare<memtable_t, memtable_t>>
            minHeap;

        std::vector<memtable_t> memtables;
        memtables.reserve(m_pStorage->size());
        for (auto it{m_pStorage->begin()}, end{m_pStorage->end()}; it != end; ++it)
        {
            auto mm = (*it)->memtable().value();
            memtables.emplace_back(mm);
        }

        int i{0};
        for (auto &memtable : memtables)
        {
            spdlog::info("memtable {}", i);
            for (auto rec : memtable)
            {
                spdlog::info("VAGAG {}", rec.m_key.m_key);
            }
            i++;
            minHeap.emplace(memtable.begin(), memtable.end());
        }

        auto memtable{memtable::memtable_t{}};
        auto lastKey{memtable::memtable_t::record_t::key_t{}};
        while (!minHeap.empty())
        {
            auto current = minHeap.top();
            minHeap.pop();

            // Add the smallest element to the merged sequence.
            // If two elements have the same key, then choose the one with the greatest timestamp
            if (memtable.empty() || lastKey != current.first->m_key)
            {
                memtable.emplace(*current.first);
                lastKey = current.first->m_key;
            }

            // Move to the next element in the current sequence
            auto next = std::next(current.first);
            if (next != current.second)
            {
                minHeap.emplace(next, current.second);
            }
        }

        // Create a new segment from the compacted segment.
        // The postfix "_compacted" signals that the segment is an intermediate result
        auto name{fmt::format("{}_{}_compacted", helpers::segment_name(), index())};
        return segments::factories::lsmtree_segment_factory(
            name, helpers::segment_path(m_pConfig->datadir_path(), name), memtable);
    }

    return nullptr;
}

void level_t::merge(const segments::regular_segment::shared_ptr_t &pSegment) noexcept
{
    // Get records of input memtable to merge them with overlapping records of the current level
    auto inMemtableRecords{pSegment->memtable().value().moved_records()};

    // Segments overlapping with input memtable
    auto overlappingSegmentsView = *m_pStorage | std::views::filter(
                                                     [](auto pSegment) {
                                                         return pSegment->min().value() > pSegment->min().value() ||
                                                                pSegment->max().value() < pSegment->max().value();
                                                     });

    std::vector<memtable_t::record_t> overlappingSegmentsRecords{};
    std::size_t overlappingSegmentsRecordsSize{};
    for (auto &outStream : overlappingSegmentsView)
    {
        overlappingSegmentsRecordsSize += outStream->memtable().value().size();
    }

    overlappingSegmentsRecords.reserve(overlappingSegmentsRecordsSize);
    for (auto &overlappingSegment : overlappingSegmentsView)
    {
        auto currentSegmentRecords{overlappingSegment->moved_memtable().value().moved_records()};
        overlappingSegmentsRecords.insert(std::end(overlappingSegmentsRecords),
                                          std::make_move_iterator(std::begin(currentSegmentRecords)),
                                          std::make_move_iterator(std::end(currentSegmentRecords)));
    }

    // Merge overlapping memtables and segments
    std::vector<memtable::memtable_t::record_t> mergedMemtable;
    std::ranges::merge(
        inMemtableRecords, overlappingSegmentsRecords, std::back_inserter(mergedMemtable), std::less<>{});

    // TODO(lnikon): Make this parameter configurable. Use measurement units(mb).
    const std::size_t segmentSize{1024};
    memtable::memtable_t newMemtable;
    segments::storage::segment_storage_t newSegments;

    for (const auto &currentRecord : mergedMemtable)
    {
        newMemtable.emplace(currentRecord);
        if (newMemtable.size() >= segmentSize)
        {
            auto name{fmt::format("{}_{}", helpers::segment_name(), index())};
            newSegments.emplace(segments::factories::lsmtree_segment_factory(
                                    name, helpers::segment_path(m_pConfig->datadir_path(), name), newMemtable),
                                segments::storage::key_range_comparator_t{});
            newMemtable = memtable::memtable_t{};
        }
    }

    // Flush leftover records
    if (!newMemtable.empty())
    {
        auto name{fmt::format("{}_{}", helpers::segment_name(), index())};
        newSegments.emplace(segments::factories::lsmtree_segment_factory(
                                name, helpers::segment_path(m_pConfig->datadir_path(), name), newMemtable),
                            segments::storage::key_range_comparator_t{});
    }

    // Delete overlapping segments after the merging process is complete
    std::ranges::for_each(overlappingSegmentsView, [this](auto pSegment) { this->purge(pSegment); });

    // Flush new segments
    for (const auto &pNewCurrentLevelSegment : newSegments)
    {
        pNewCurrentLevelSegment->flush();
        emplace(pNewCurrentLevelSegment);
    }
}

void level_t::purge() const noexcept
{
    assert(m_pStorage);
    spdlog::info("Purging level {} with {} segments", index(), m_pStorage->size());
    purge(*m_pStorage);
}

void level_t::purge(const segments::types::name_t &segment_name) const noexcept
{
    assert(m_pStorage);
    if (auto segmentIt{std::find_if(m_pStorage->begin(),
                                    m_pStorage->end(),
                                    [&segment_name](auto pSegment) { return pSegment->get_name() == segment_name; })};
        segmentIt != m_pStorage->end())
    {
        purge(*segmentIt);
    }
}

void level_t::purge(segments::storage::segment_storage_t &storage) const noexcept
{
    // Go over the old segments and remove them from disk
    std::vector<segments::regular_segment::shared_ptr_t> segments;
    segments.reserve(storage.size());
    std::copy(std::begin(storage), std::end(storage), std::back_inserter(segments));

    for (const auto &currentSegment : segments)
    {
        assert(currentSegment);
        purge(currentSegment);
    }

    // Clear the in-memory segments storage
    spdlog::info("Clear the in-memory segments storage");
    storage.clear();
}

void level_t::purge(const segments::regular_segment::shared_ptr_t &pSegment) const noexcept
{
    assert(pSegment);

    spdlog::info("Removing segment {} from level {}", pSegment->get_name(), index());

    m_manifest->add(db::manifest::manifest_t::segment_record_t{
        .op = segment_operation_k::remove_segment_k, .name = pSegment->get_name(), .level = index()});

    pSegment->purge();
    m_pStorage->remove(pSegment);
}

auto level_t::index() const noexcept -> level_t::level_index_type_t
{
    return m_levelIndex;
}

auto level_t::storage() -> segments::storage::shared_ptr_t
{
    return m_pStorage;
}
} // namespace structures::lsmtree::level
