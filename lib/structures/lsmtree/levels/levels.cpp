//
// Created by nikon on 3/8/24.
//

#include "levels.h"

#include <spdlog/spdlog.h>

namespace structures::lsmtree::levels
{

// ====================
// compaction_trigger_t
// ====================
struct compaction_trigger_t
{
    /**
     * @brief
     *
     * @param pLevel
     */
    compaction_trigger_t(level::shared_ptr_t pLevel);

    /**
     * @brief
     *
     * @return
     */
    virtual bool trigger() const noexcept = 0;

  private:
    level::shared_ptr_t m_pLevel{nullptr};
};

compaction_trigger_t::compaction_trigger_t(level::shared_ptr_t pLevel)
    : m_pLevel{pLevel}
{
}

// ========
// levels_t
// ========
levels_t::levels_t(const config::shared_ptr_t pConfig) noexcept
    : m_pConfig{pConfig}
{
}

segments::interface::shared_ptr_t levels_t::segment(const structures::lsmtree::lsmtree_segment_type_t type,
                                                    memtable::memtable_t memtable)
{
    // Create level zero if it doesn't exist
    if (m_levels.empty())
    {
        level();
    }

    // Create a new segment for the memtable that became immutable
    assert(m_levels[0]);
    auto pSegment = m_levels[0]->segment(type, std::move(memtable));
    assert(pSegment);

    segments::interface::shared_ptr_t compactedCurrentLevelSegment{nullptr};
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
                std::cout << "skipping compactation of levels >= 1" << std::endl;
                break;
            }

            std::cout << "skipping compactation of level = " << currentLevel->index() << std::endl;
            continue;
        }

        assert(compactedCurrentLevelSegment);

        // If current level is compacted, we can merge it with the nextLevel
        if (compactedCurrentLevelSegment)
        {
            // If compactation succeeded, then flush the compacted segment into disk
            compactedCurrentLevelSegment->flush();

            // Create next level if it doesn't exist
            level::shared_ptr_t nextLevel{nullptr};
            if (currentLevel->index() + 1 == m_levels.size())
            {
                nextLevel = level();
                assert(nextLevel);
            }
            else
            {
                nextLevel = level(currentLevel->index() + 1);
            }

            // Merge compacted @currentLevel into the @nextLevel
            nextLevel->merge(compactedCurrentLevelSegment);

            // After merging current level into the next level purge it
            // Purge the segment representing the compacted level as well
            compactedCurrentLevelSegment->purge();
            currentLevel->purge();
        }
    }

    // If compactation happened, then return the resulting segment
    return compactedCurrentLevelSegment ? compactedCurrentLevelSegment : pSegment;
}

level::shared_ptr_t levels_t::level() noexcept
{
    return m_levels.emplace_back(level::make_shared(m_levels.size(), m_pConfig));
}

/**
 * @brief
 *
 * @param idx
 */
[[maybe_unused]] level::shared_ptr_t levels_t::level(const std::size_t idx) noexcept
{
    assert(idx < m_levels.size());
    return m_levels[idx];
}

std::optional<record_t> levels_t::record(const key_t &key) const noexcept
{
    std::optional<record_t> result{};
    for (const auto level : m_levels)
    {
        assert(level);
        result = level->record(key);
        if (result)
        {
            spdlog::info("Found key {} at level {}", key.m_key, level->index());
            break;
        }
    }
    return result;
}

} // namespace structures::lsmtree::levels
