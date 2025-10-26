#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <optional>
#include <queue>
#include <ranges>
#include <utility>
#include <cmath>

#include <spdlog/spdlog.h>
#include <absl/debugging/stacktrace.h>

#include "structures/lsmtree/levels/level.h"
#include "structures/lsmtree/lsmtree_types.h"
#include "structures/lsmtree/segments/lsmtree_segment_factory.h"
#include "structures/lsmtree/segments/segment_storage.h"
#include "structures/lsmtree/segments/helpers.h"
#include "structures/memtable/memtable.h"

namespace structures::lsmtree::level
{

namespace helpers = segments::helpers;

using segment_operation_k = db::manifest::manifest_t::segment_record_t::operation_k;

template <typename T, typename U = T> struct IteratorCompare
{
    auto operator()(
        const std::pair<typename T::const_iterator, typename T::const_iterator> &lhs,
        const std::pair<typename U::const_iterator, typename U::const_iterator> &rhs
    ) const -> bool
    {
        return *lhs.first > *rhs.first;
    }
};

level_t::level_t(
    const level_index_type_t   levelIndex,
    config::shared_ptr_t       pConfig,
    db::manifest::shared_ptr_t manifest
) noexcept
    : m_levelIndex{levelIndex},
      m_pConfig{std::move(pConfig)},
      m_manifest{std::move(std::move(manifest))}
{
}

void level_t::emplace(const lsmtree::segments::regular_segment::shared_ptr_t &pSegment) noexcept
{
    assert(pSegment);
    spdlog::debug("Adding segment {} into level {}", pSegment->get_name(), index());

    if (index() == 0)
    {
        m_storage.emplace(pSegment, segments::storage::last_write_time_comparator_t{});
    }
    else
    {
        m_storage.emplace(pSegment, segments::storage::key_range_comparator_t{});
    }

    ASSERT(m_manifest->add(
        db::manifest::manifest_t::segment_record_t{
            .op = segment_operation_k::add_segment_k, .name = pSegment->get_name(), .level = index()
        }
    ));
}

auto level_t::segment(memtable::memtable_t memtable) -> segments::regular_segment::shared_ptr_t
{
    // Generate name for the segment
    auto name{fmt::format("{}_{}", helpers::segment_name(), index())};
    return segment(std::move(memtable), name);
}

auto level_t::segment(memtable::memtable_t memtable, const std::string &name)
    -> segments::regular_segment::shared_ptr_t
{
    absl::WriterMutexLock lock{&m_mutex};

    // Generate a path for the segment, including its name, then based on @type
    // and @pMemtable create a segment
    auto pSegment{segments::factories::lsmtree_segment_factory(
        name, helpers::segment_path(m_pConfig->datadir_path(), name), std::move(memtable)
    )};
    assert(pSegment);

    // Add the segment to the storage and flush it to disk
    emplace(pSegment);
    pSegment->flush();

    return pSegment;
}

auto level_t::record(const key_t &key) const noexcept
    -> std::optional<memtable::memtable_t::record_t>
{
    absl::ReaderMutexLock lock{&m_mutex};
    for (const auto &pSegment : m_storage)
    {
        if (const auto &result = pSegment->record(key); !result.empty())
        {
            spdlog::debug(
                "Found record {} at level {} in segment {}",
                key.m_key,
                index(),
                pSegment->get_name()
            );
            return result[0];
        }
    }
    return std::nullopt;
}

auto level_t::compact() const noexcept -> segments::regular_segment::shared_ptr_t
{
    absl::ReaderMutexLock lock{&m_mutex};

    // Skip compaction if size of the level is less than the threshold
    const std::uint64_t used{bytes_used()};
    const std::uint64_t threshold{
        index() == 0 ? m_pConfig->LSMTreeConfig.LevelZeroCompactionThreshold
                     : m_pConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold *
                           static_cast<std::uint64_t>(std::pow(10, index()))
    };
    if (used < threshold)
    {
        spdlog::info(
            "Skipping level={} compaction. Used={}, threshold={}", index(), used, threshold
        );
        return nullptr;
    }

    spdlog::info("Compacting level={}. Used={}, threshold={}", index(), used, threshold);

    // Merge segments from the current level
    std::priority_queue<
        std::pair<
            typename memtable::memtable_t::const_iterator,
            typename memtable::memtable_t::const_iterator>,
        std::vector<std::pair<
            typename memtable::memtable_t::const_iterator,
            typename memtable::memtable_t::const_iterator>>,
        IteratorCompare<memtable_t>>
        minHeap;

    for (const auto &segment : m_storage)
    {
        // TODO(lnikon): reset memtable inside regular_segment_t at the of the flush() and recover
        // it here e.g. segment->recover_memtable();
        const auto &currentMemtable = segment->memtable().value();
        minHeap.emplace(currentMemtable.begin(), currentMemtable.end());
    }

    auto mergedMemtable{memtable::memtable_t{}};
    auto lastKey{memtable::memtable_t::record_t::key_t{}};
    while (!minHeap.empty())
    {
        auto current = minHeap.top();
        minHeap.pop();

        // Add the smallest element to the merged sequence.
        // If two elements have the same key, then choose the one with the greatest timestamp
        if (mergedMemtable.empty() || lastKey != current.first->m_key)
        {
            mergedMemtable.emplace(*current.first);
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
        name, helpers::segment_path(m_pConfig->datadir_path(), name), std::move(mergedMemtable)
    );
}

void level_t::merge(const segments::regular_segment::shared_ptr_t &pSegment) noexcept
{
    absl::WriterMutexLock nextLevelLock{&m_mutex};

    // Get records of input memtable to merge them with overlapping records of
    // the current level
    auto inMemtableRecords{pSegment->memtable().value().moved_records()};

    // Segments overlapping with input memtable
    auto overlappingSegmentsView =
        m_storage | std::views::filter(
                        [pSegment](const auto &currentSegment)
                        {
                            return !(
                                currentSegment->max().value() < pSegment->min().value() ||
                                currentSegment->min().value() > pSegment->max().value()
                            );
                        }
                    );

    // Calculate total number of records in memtables overlapping with @pSegment
    std::size_t overlappingSegmentsRecordsCount{0};
    for (auto &outStream : overlappingSegmentsView)
    {
        overlappingSegmentsRecordsCount += outStream->memtable().value().size();
    }

    // Store records of memtables overlapping with @pSegment into
    // @overlappingSegmentsRecords
    std::vector<memtable_t::record_t> overlappingSegmentsRecords{};
    overlappingSegmentsRecords.reserve(overlappingSegmentsRecordsCount);
    for (auto &overlappingSegment : overlappingSegmentsView)
    {
        auto currentSegmentRecords{overlappingSegment->moved_memtable().value().moved_records()};
        overlappingSegmentsRecords.insert(
            std::end(overlappingSegmentsRecords),
            std::make_move_iterator(std::begin(currentSegmentRecords)),
            std::make_move_iterator(std::end(currentSegmentRecords))
        );
    }

    // Merge overlapping memtables and segments
    std::vector<memtable::memtable_t::record_t> mergedMemtable;
    std::ranges::merge(
        inMemtableRecords,
        overlappingSegmentsRecords,
        std::back_inserter(mergedMemtable),
        std::less<>{}
    );

    // TODO(lnikon): Make this parameter configurable. Use measurement
    // units(mb).
    memtable::memtable_t                 newMemtable;
    segments::storage::segment_storage_t newSegments;

    for (const auto &currentRecord : mergedMemtable)
    {
        newMemtable.emplace(currentRecord);
        // TODO(lnikon): Make SegmentSize configurall.
        // if (newMemtable.size() >= m_pConfig->LSMTreeConfig.SegmentSize)
        if (newMemtable.size() >= 1024)
        {
            auto name{fmt::format("{}_{}", helpers::segment_name(), index())};
            newSegments.emplace(
                segments::factories::lsmtree_segment_factory(
                    name, helpers::segment_path(m_pConfig->datadir_path(), name), newMemtable
                ),
                segments::storage::key_range_comparator_t{}
            );
            newMemtable = memtable::memtable_t{};
        }
    }

    // Flush leftover records
    if (!newMemtable.empty())
    {
        auto name{fmt::format("{}_{}", helpers::segment_name(), index())};
        newSegments.emplace(
            segments::factories::lsmtree_segment_factory(
                name, helpers::segment_path(m_pConfig->datadir_path(), name), newMemtable
            ),
            segments::storage::key_range_comparator_t{}
        );
    }

    // Delete overlapping segments after the merging process is complete
    std::ranges::for_each(
        overlappingSegmentsView, [this](auto pSegment) { this->purge(pSegment); }
    );

    // Flush new segments
    for (const auto &pNewCurrentLevelSegment : newSegments)
    {
        pNewCurrentLevelSegment->flush();
        emplace(pNewCurrentLevelSegment);
    }
}

void level_t::purge() noexcept
{
    absl::WriterMutexLock lock{&m_mutex};
    spdlog::info("Purging level {} with {} segments", index(), m_storage.size());
    const auto idx{index()};
    for (auto &pSegment : m_storage)
    {
        pSegment->remove_from_disk();

        auto ok{m_manifest->add(
            db::manifest::manifest_t::segment_record_t{
                .op = segment_operation_k::remove_segment_k,
                .name = pSegment->get_name(),
                .level = idx
            }
        )};
        ASSERT(ok);
    }
    m_storage.clear();
}

auto level_t::purge(const segments::types::name_t &segmentName) noexcept -> void
{
    if (auto pSegment{m_storage.find(segmentName)}; pSegment)
    {
        purge(pSegment);
    }
    else
    {
        spdlog::warn(
            "Unable to find segment with name {} at level {} to purge", segmentName, index()
        );
    }
}

void level_t::purge(const segments::regular_segment::shared_ptr_t &pSegment) noexcept
{
    absl::WriterMutexLock lock{&m_mutex};
    assert(pSegment);

    spdlog::debug("Removing segment {} from level {}", pSegment->get_name(), index());

    ASSERT(m_manifest->add(
        db::manifest::manifest_t::segment_record_t{
            .op = segment_operation_k::remove_segment_k,
            .name = pSegment->get_name(),
            .level = index()
        }
    ));

    pSegment->remove_from_disk();
    m_storage.remove(pSegment);
}

auto level_t::index() const noexcept -> level_t::level_index_type_t
{
    return m_levelIndex;
}

auto level_t::bytes_used() const noexcept -> std::size_t
{
    /*absl::ReaderMutexLock lock{&m_mutex};*/
    std::size_t result{0};
    for (const auto &pSegment : m_storage)
    {
        result += pSegment->num_of_bytes_used();
    }
    return result;
}

auto level_t::restore() noexcept -> void
{
    for (auto &pSegment : m_storage)
    {
        pSegment->restore();
    }
}

} // namespace structures::lsmtree::level
