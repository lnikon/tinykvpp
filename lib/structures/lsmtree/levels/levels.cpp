//
// Created by nikon on 3/8/24.
//

#include "levels.h"
#include <db/manifest/manifest.h>

#include <spdlog/spdlog.h>

#include <utility>

namespace structures::lsmtree::levels
{

using level_operation_k = db::manifest::manifest_t::level_record_t::operation_k;
using segment_operation_k = db::manifest::manifest_t::segment_record_t::operation_k;

// ========
// levels_t
// ========
levels_t::levels_t(config::shared_ptr_t pConfig, db::manifest::shared_ptr_t manifest) noexcept
    : m_pConfig{std::move(pConfig)},
      m_manifest{std::move(std::move(manifest))}
{
}

auto levels_t::segment(const structures::lsmtree::lsmtree_segment_type_t type,
                       memtable::memtable_t memtable) -> segments::regular_segment::shared_ptr_t
{
    // Create level zero if it doesn't exist
    if (m_levels.empty())
    {
        m_manifest->add(db::manifest::manifest_t::level_record_t{.op = level_operation_k::add_level_k, .level = 0});
        level();
    }

    // Create a new segment for the memtable that became immutable
    assert(m_levels[0]);
    auto pSegment = m_levels[0]->segment(type, memtable);
    assert(pSegment);

    // Update manifest with new segment
    m_manifest->add(db::manifest::manifest_t::segment_record_t{
        .op = segment_operation_k::add_segment_k, .name = pSegment->get_name(), .level = 0});

    segments::regular_segment::shared_ptr_t compactedCurrentLevelSegment{nullptr};
    for (std::size_t idx{0}; idx < m_levels.size(); idx++)
    {
        auto currentLevel{m_levels[idx]};
        assert(currentLevel);

        // Try to compact the @currentLevel
        compactedCurrentLevelSegment = currentLevel->compact();

        // If 0th level is not ready for the compactation, then skip the other levels
        if (!compactedCurrentLevelSegment)
        {
            if (currentLevel->index() == 0)
            {
                // std::cout << "skipping compactation of levels >= 1" << std::endl;
                break;
            }

            // std::cout << "skipping compactation of level = " << currentLevel->index() << std::endl;
            continue;
        }

        assert(compactedCurrentLevelSegment);

        // If current level is compacted, we can merge it with the nextLevel
        if (compactedCurrentLevelSegment)
        {
            // Update manifest with compacted level
            m_manifest->add(db::manifest::manifest_t::level_record_t{.op = level_operation_k::compact_level_k,
                                                                     .level = currentLevel->index()});

            // Update manifest with new segment
            m_manifest->add(db::manifest::manifest_t::segment_record_t{.op = segment_operation_k::add_segment_k,
                                                                       .name = compactedCurrentLevelSegment->get_name(),
                                                                       .level = currentLevel->index()});

            // If compactation succeeded, then flush the compacted segment into disk
            compactedCurrentLevelSegment->flush();

            // Create next level if it doesn't exist
            level::shared_ptr_t nextLevel{nullptr};
            if (currentLevel->index() + 1 == m_levels.size())
            {
                nextLevel = level();
                m_manifest->add(db::manifest::manifest_t::level_record_t{.op = level_operation_k::add_level_k,
                                                                         .level = nextLevel->index()});
                assert(nextLevel);
            }
            else
            {
                nextLevel = level(currentLevel->index() + 1);
            }

            // Merge compacted @currentLevel into the @nextLevel
            nextLevel->merge(compactedCurrentLevelSegment);

            // Purge the segment representing the compacted level and update the manifest
            m_manifest->add(db::manifest::manifest_t::segment_record_t{.op = segment_operation_k::remove_segment_k,
                                                                       .name = compactedCurrentLevelSegment->get_name(),
                                                                       .level = currentLevel->index()});
            compactedCurrentLevelSegment->purge();

            // After merging current level into the next level purge the current level and update the manifest
            m_manifest->add(db::manifest::manifest_t::level_record_t{.op = level_operation_k::purge_level_k,
                                                                     .level = currentLevel->index()});
            currentLevel->purge();
        }
    }

    m_manifest->print();

    // If compactation happened, then return the resulting segment
    return compactedCurrentLevelSegment ? compactedCurrentLevelSegment : pSegment;
}

auto levels_t::level() noexcept -> level::shared_ptr_t
{
    return m_levels.emplace_back(level::make_shared(m_levels.size(), m_pConfig, m_manifest));
}

/**
 * @brief
 *
 * @param idx
 */
[[maybe_unused]] auto levels_t::level(const std::size_t idx) noexcept -> level::shared_ptr_t
{
    assert(idx < m_levels.size());
    return m_levels[idx];
}

auto levels_t::size() const noexcept -> levels_t::levels_storage_t::size_type
{
    return m_levels.size();
}

auto levels_t::record(const key_t &key) const noexcept -> std::optional<record_t>
{
    std::optional<record_t> result{};
    for (const auto &currentLevel : m_levels)
    {
        assert(currentLevel);
        result = currentLevel->record(key);
        if (result)
        {
            spdlog::info("Found key {} at level {}", key.m_key, currentLevel->index());
            break;
        }
    }
    return result;
}

} // namespace structures::lsmtree::levels
