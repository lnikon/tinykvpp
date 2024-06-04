#include "level.h"
#include "structures/lsmtree/lsmtree_types.h"
#include "structures/lsmtree/segments/segment_storage.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <structures/lsmtree/segments/helpers.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>

#include <ranges>

namespace structures::lsmtree::level
{

namespace factories = segments::factories;
namespace helpers = segments::helpers;

level_t::level_t(const config::shared_ptr_t pConfig) noexcept
    : m_pConfig{pConfig},
      m_pStorage{segments::storage::make_shared()}
{
}

void level_t::emplace(lsmtree::segments::interface::shared_ptr_t pSegment) noexcept
{
    m_pStorage->emplace(pSegment, segments::storage::last_write_time_comparator_t{});
}

segments::interface::shared_ptr_t level_t::segment(const lsmtree_segment_type_t type, memtable::memtable_t pMemtable)
{
    const auto name{helpers::segment_name()};
    auto pSegment{factories::lsmtree_segment_factory(
        type, name, helpers::segment_path(m_pConfig->datadir_path(), name), std::move(pMemtable))};

    // Store newly created segment into the storage
    emplace(pSegment);

    // Flush newly created segment
    // TODO(lnikon): Can be done async?
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

    std::cout << "level compacted" << std::endl;

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

    // Segments overlapping with input memtable
    auto overlappingSegmentsView = *m_pStorage | std::views::filter(
                                                     [](auto pSegment) {
                                                         return pSegment->min().value() > pSegment->min().value() ||
                                                                pSegment->max().value() < pSegment->max().value();
                                                     });

    auto memtablesJoined = overlappingSegmentsView |
                           std::views::transform([](auto pSegment) { return pSegment->memtable().value(); }) |
                           std::views::join;

    // Merge overlapping memtables and segments
    std::vector<memtable::memtable_t::record_t> mergedMemtable;
    std::ranges::merge(inMemtableView,
                       memtablesJoined,
                       std::back_inserter(mergedMemtable),
                       [](auto &&lhs, auto &&rhs) { return lhs < rhs; });

    // TODO(lnikon): Make this parameter configurable. Use measurement units(mb).
    const std::size_t segmentSize{10};
    memtable::memtable_t newMemtable;
    segments::storage::segment_storage_t newSegments;
    std::ranges::for_each(memtablesJoined,
                          [&newMemtable, &newSegments, this](auto &&record)
                          {
                              newMemtable.emplace(std::move(record));
                              if (newMemtable.size() >= segmentSize)
                              {
                                  auto name{helpers::segment_name() + segments::types::name_t{"_compacted"}};
                                  newSegments.emplace(factories::lsmtree_segment_factory(
                                                          m_pConfig->LSMTreeConfig.SegmentType,
                                                          std::move(name),
                                                          helpers::segment_path(m_pConfig->datadir_path(), name),
                                                          std::move(newMemtable)),
                                                      // TODO(lnikon): Use min/max comparator
                                                      segments::storage::last_write_time_comparator_t{});
                              }
                          });

    // Flush new segments
    for (auto pCurrentLevelSegment : newSegments)
    {
        pCurrentLevelSegment->flush();
    }

    // Delete overlapping segments after the merging process is complete
    std::ranges::for_each(overlappingSegmentsView, [this](auto pSegment) { this->purge(pSegment); });

    return nullptr;
}

void level_t::purge() const noexcept
{
    purge(*m_pStorage);
}

void level_t::purge(segments::storage::segment_storage_t &storage) const noexcept
{
    // Go over the old segments and remove them from disk
    for (auto &segment : storage)
    {
        purge(segment);
    }

    // Clear the in-memory segments storage
    storage.clear();
}

void level_t::purge(segments::interface::shared_ptr_t pSegment) const noexcept
{
    if (std::filesystem::exists(pSegment->get_path()))
    {
        std::cout << "[level_t::purge]: "
                  << "Removing old segment \"" << pSegment->get_path() << "\"" << std::endl;
        std::filesystem::remove(pSegment->get_path());
    }
}
} // namespace structures::lsmtree::level
