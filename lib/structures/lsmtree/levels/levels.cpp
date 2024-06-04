//
// Created by nikon on 3/8/24.
//

#include "levels.h"

#include <iterator>

namespace structures::lsmtree::levels
{

levels_t::levels_t(const config::shared_ptr_t pConfig) noexcept
    : m_pConfig{pConfig}
{
}

segments::interface::shared_ptr_t levels_t::segment(const structures::lsmtree::lsmtree_segment_type_t type,
                                                    memtable::memtable_t memtable)
{
    assert(pMemtable);

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
    std::cout << "# of levels: " << m_levels.size() << std::endl;
    for (std::size_t idx{0}; idx < m_levels.size(); ++idx)
    {
        auto currentLevel{m_levels[idx]};
        assert(currentLevel);

        // Try to compact the @currentLevel
        std::cout << "trying to compact current level..." << std::endl;
        compactedCurrentLevelSegment = currentLevel->compact();
        std::cout << "...done" << std::endl;

        // If 0th level is not ready for the compactation, then skip the other levels
        // TODO(lnikon): Add level_idx property to level
        if (!compactedCurrentLevelSegment && idx == 0)
        {
            std::cout << "skipping compactation of levels >= 1" << std::endl;
            break;
        }

        assert(compactedSegment);

        // If current level is compacted, we can merge it with the nextLevel
        if (compactedCurrentLevelSegment)
        {
            // If compactation succeeded, then flush the compacted segment into disk
            std::cout << "compacted level flushed" << std::endl;
            compactedCurrentLevelSegment->flush();

            // Create next level if it doesn't exist
            level::shared_ptr_t nextLevel{nullptr};
            if (idx + 1 == m_levels.size())
            {
                std::cout << "dynamically creating level..." << std::endl;
                nextLevel = level();
                assert(nextLevel);
                std::cout << "# of levels: " << m_levels.size() << std::endl;
                std::cout << "...done" << std::endl;
            }

            // Remove old segments from memory and disk
            std::cout << "purging current level..." << std::endl;
            currentLevel->purge();
            std::cout << "...done" << std::endl;

            // Add the new segment into the storage
            // Is it necessary to temporaryly store the segment on its level despite we're in the process of the
            // compactation
            currentLevel->emplace(compactedCurrentLevelSegment);

            // Merge compacted @currentLevel into the @nextLevel
            std::cout << "merging current level with previous one..." << std::endl;
            nextLevel->merge(compactedCurrentLevelSegment);
            std::cout << "...done" << std::endl;
        }
    }

    std::cout << "finished compactations" << std::endl;

    // If compactation happened, then return the resulting segment
    return compactedCurrentLevelSegment ? compactedCurrentLevelSegment : pSegment;
}

level::shared_ptr_t levels_t::level() noexcept
{
    return m_levels.emplace_back(level::make_shared(m_pConfig));
}

std::optional<record_t> levels_t::record(const key_t &key) const noexcept
{
    std::optional<record_t> result{};
    for (const auto level : m_levels)
    {
        assert(level);
        result = level->record(key);
        if (!result)
        {
            continue;
        }
    }
    return result;
}

} // namespace structures::lsmtree::levels
