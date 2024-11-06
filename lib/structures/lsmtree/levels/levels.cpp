//
// Created by nikon on 3/8/24.
//

#include "levels.h"
#include <absl/synchronization/mutex.h>
#include <cassert>
#include <db/manifest/manifest.h>
#include "../segments/helpers.h"

#include <utility>

#include <spdlog/spdlog.h>

namespace structures::lsmtree::levels
{

using level_operation_k = db::manifest::manifest_t::level_record_t::operation_k;
using segment_operation_k = db::manifest::manifest_t::segment_record_t::operation_k;

levels_t::levels_t(config::shared_ptr_t pConfig, db::manifest::shared_ptr_t pManifest) noexcept
    : m_pConfig{std::move(pConfig)},
      m_pManifest{std::move(std::move(pManifest))},
      m_compaction_thread(
          [this](std::stop_token stoken)
          {
              while (true)
              {
                  if (stoken.stop_requested())
                  {
                      return;
                  }

                  if (m_level0_segment_flushed_notification.WaitForNotificationWithTimeout(absl::Seconds(1)))
                  {
                      compact();
                  }
                  else
                  {
                      continue;
                  }
              }
          })
{
    // TODO: Make number of levels configurable
    // const std::size_t levelCount{m_pConfig->LSMTreeConfig.LevelCount};
    const std::size_t levelCount{7};
    for (std::size_t idx{0}; idx < levelCount; idx++)
    {
        m_pManifest->add(db::manifest::manifest_t::level_record_t{.op = level_operation_k::add_level_k, .level = idx});
        level();
    }
}

levels_t::~levels_t() noexcept
{
    m_compaction_thread.request_stop();
    m_compaction_thread.join();
}

auto levels_t::compact() -> segments::regular_segment::shared_ptr_t
{
    absl::MutexLock lock{&m_mutex};

    segments::regular_segment::shared_ptr_t compactedCurrentLevelSegment{nullptr};
    for (std::size_t idx{0}; idx < m_levels.size(); idx++)
    {
        auto currentLevel{m_levels[idx]};
        assert(currentLevel);

        // Do not compact the last level
        if (currentLevel->index() == m_levels.size() - 1)
        {
            break;
        }

        // Try to compact the @currentLevel
        compactedCurrentLevelSegment = currentLevel->compact();

        // If 0th level is not ready for the compaction, then skip the other levels
        if (!compactedCurrentLevelSegment)
        {
            if (currentLevel->index() == 0)
            {
                break;
            }

            continue;
        }

        // Update manifest with compacted level
        m_pManifest->add(db::manifest::manifest_t::level_record_t{.op = level_operation_k::compact_level_k,
                                                                  .level = currentLevel->index()});

        // Update manifest with new segment
        m_pManifest->add(db::manifest::manifest_t::segment_record_t{.op = segment_operation_k::add_segment_k,
                                                                    .name = compactedCurrentLevelSegment->get_name(),
                                                                    .level = currentLevel->index()});

        // If computation succeeded, then flush the compacted segment into disk
        compactedCurrentLevelSegment->flush();

        // Get the next level
        level::shared_ptr_t nextLevel = m_levels[currentLevel->index() + 1];
        assert(nextLevel);

        // Lock the next level and merge current compacted level into it
        {
            nextLevel->merge(compactedCurrentLevelSegment);
        }

        // Purge the segment representing the compacted level and update the manifest
        m_pManifest->add(db::manifest::manifest_t::segment_record_t{.op = segment_operation_k::remove_segment_k,
                                                                    .name = compactedCurrentLevelSegment->get_name(),
                                                                    .level = currentLevel->index()});
        compactedCurrentLevelSegment->remove_from_disk();

        // After merging current level into the next level purge the current level and update the manifest
        m_pManifest->add(db::manifest::manifest_t::level_record_t{.op = level_operation_k::purge_level_k,
                                                                  .level = currentLevel->index()});
        currentLevel->purge();
    }

    // If compaction happened, then return the resulting segment
    return compactedCurrentLevelSegment;
}

auto levels_t::level() noexcept -> level::shared_ptr_t
{
    absl::MutexLock lock{&m_mutex};
    return m_levels.emplace_back(level::make_shared(m_levels.size(), m_pConfig, m_pManifest));
}

[[maybe_unused]] auto levels_t::level(const std::size_t idx) noexcept -> level::shared_ptr_t
{
    assert(idx < m_levels.size());
    return m_levels[idx];
}

auto levels_t::record(const key_t &key) const noexcept -> std::optional<record_t>
{
    absl::MutexLock lock{&m_mutex};

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

auto levels_t::size() const noexcept -> levels_t::levels_storage_t::size_type
{
    absl::MutexLock lock{&m_mutex};
    return m_levels.size();
}

[[nodiscard]] auto
levels_t::flush_to_level0(memtable::memtable_t memtable) const noexcept -> segments::regular_segment::shared_ptr_t
{
    absl::MutexLock lock{&m_mutex};

    assert(m_levels[0]);

    // Generate name for the segment and add it to the manifest
    auto name{fmt::format("{}_{}", segments::helpers::segment_name(), 0)};
    m_pManifest->add(
        db::manifest::manifest_t::segment_record_t{.op = segment_operation_k::add_segment_k, .name = name, .level = 0});

    auto pSegement{m_levels[0]->segment(std::move(memtable), name)};
    if (pSegement)
    {
        if (!m_level0_segment_flushed_notification.HasBeenNotified())
        {
            m_level0_segment_flushed_notification.Notify();
        }
    }
    return pSegement;
}

auto levels_t::restore() noexcept -> void
{
    absl::WriterMutexLock lock{&m_mutex};
    for (auto &level : m_levels)
    {
        level->restore();
    }
}

} // namespace structures::lsmtree::levels
