#include "db/wal/wal.h"
#include <absl/synchronization/mutex.h>
#include <cassert>
#include <db/manifest/manifest.h>
#include <sstream>
#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/helpers.h>
#include <structures/lsmtree/lsmtree.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>

#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

namespace structures::lsmtree
{

using level_operation_k = db::manifest::manifest_t::level_record_t::operation_k;
using segment_operation_k = db::manifest::manifest_t::segment_record_t::operation_k;

/**
 * ============================================================================
 * Public interface
 * ============================================================================
 */

lsmtree_t::lsmtree_t(const config::shared_ptr_t &pConfig,
                     db::manifest::shared_ptr_t  pManifest,
                     db::wal::shared_ptr_t       pWal) noexcept
    : m_pConfig{pConfig},
      m_table{std::make_optional<memtable::memtable_t>()},
      m_pManifest{std::move(pManifest)},
      m_pWal{std::move(pWal)},
      m_levels{pConfig, m_pManifest}
{
}

void lsmtree_t::put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value) noexcept
{
    assert(m_pConfig);

    absl::WriterMutexLock lock{&m_mutex};
    assert(m_table);

    // Record addition of the new key into the WAL and add record into memtable
    auto record{record_t{key, value}};
    m_pWal->add({db::wal::wal_t::operation_k::add_k, record});
    m_table->emplace(std::move(record));

    if (m_table->size() >= m_pConfig->LSMTreeConfig.DiskFlushThresholdSize)
    {
        // Move the current memtable to the flushing queue for disk persistence
        m_flushing_queue.push(std::move(m_table.value()));

        // Create a new empty memtable to replace the old one
        m_table = std::make_optional<memtable::memtable_t>();

        // Reset the Write-Ahead Log (WAL) to start logging anew
        m_pWal->reset();
    }
}

auto lsmtree_t::get(const key_t &key) noexcept -> std::optional<record_t>
{
    absl::ReaderMutexLock lock{&m_mutex};
    assert(m_table);

    // TODO(lnikon): Skip searching if record doesn't exist
    //    const auto recordExists{m_bloom.exists(key)};
    //    if (!recordExists)
    //    {
    //        return std::nullopt;
    //    }

    // If bloom check passed, then record probably exists.
    // Lookup in-memory table for the table
    auto result{m_table->find(key)};

    // If key isn't in in-memory table, then it probably was flushed.
    // Lookup for the key in on-disk segments
    if (!result.has_value())
    {
        result = m_levels.record(key);
    }

    return result;
}

auto lsmtree_t::recover() noexcept -> bool
{
    assert(m_pConfig);
    assert(m_pManifest);

    // Disable all updates to manifest in recovery phase
    // because nothing new is happening, lsmtree is re-using already
    // existing information
    m_pManifest->disable();

    // Restore lsmtree structure from the manifest file
    if (!restore_manifest())
    {
        spdlog::error("Unable to restore manifest at {}", m_pManifest->path().c_str());
        return false;
    }

    // Restore memtable from WAL
    if (!restore_wal())
    {
        spdlog::error("Unable to restore WAL at {}", m_pWal->path().c_str());
        return false;
    }

    // Enable updates to manifest after recovery is finished
    m_pManifest->enable();

    return true;
}

/**
 * ============================================================================
 * Private, utility functions
 * ============================================================================
 */

auto lsmtree_t::restore_manifest() noexcept -> bool
{
    const auto &records{m_pManifest->records()};
    for (const auto &record : records)
    {
        std::visit(
            [this](auto record)
            {
                using T = std::decay_t<decltype(record)>;
                if constexpr (std::is_same_v<T, db::manifest::manifest_t::segment_record_t>)
                {
                    switch (record.op)
                    {
                    case segment_operation_k::add_segment_k:
                    {
                        m_levels.level(record.level)
                            ->emplace(segments::factories::lsmtree_segment_factory(
                                record.name,
                                segments::helpers::segment_path(m_pConfig->datadir_path(), record.name),
                                memtable_t{}));
                        spdlog::debug("Segment {} added into level {} during recovery", record.name, record.level);
                        break;
                    }
                    case segment_operation_k::remove_segment_k:
                    {
                        m_levels.level(record.level)->purge(record.name);
                        spdlog::info("Segment {} removed from level {} during recovery", record.name, record.level);
                        break;
                    }
                    default:
                    {
                        spdlog::error("Unknown segment operation={}", static_cast<std::int32_t>(record.op));
                        break;
                    }
                    }
                }
                else if constexpr (std::is_same_v<T, db::manifest::manifest_t::level_record_t>)
                {
                    switch (record.op)
                    {
                    case level_operation_k::add_level_k:
                    {
                        m_levels.level();
                        spdlog::info("Level {} created during recovery", record.level);
                        break;
                    }
                    case level_operation_k::compact_level_k:
                    {
                        spdlog::info(
                            "Ignoring {} during recovery",
                            db::manifest::manifest_t::level_record_t::ToString(level_operation_k::compact_level_k));
                        break;
                    }
                    case level_operation_k::purge_level_k:
                    {
                        spdlog::info(
                            "Ignoring {} during recovery",
                            db::manifest::manifest_t::level_record_t::ToString(level_operation_k::purge_level_k));
                        break;
                    }
                    default:
                    {
                        spdlog::error("Unknown level operation={}", static_cast<std::int32_t>(record.op));
                        break;
                    }
                    }
                }
                else
                {
                    spdlog::error("Unknown manifest record type");
                    assert(false);
                }
            },
            record);
    }

    // A little bit of printbugging :)
    // Now is that all modifications are applied to the segments storage it is time to restore segments.
    // This will repopulate indices
    spdlog::info("Restoring in-memory indices");
    const auto  level_count{m_levels.size()};
    std::size_t current_level_idx{0};
    while (current_level_idx != level_count)
    {
        spdlog::info("Restoring level {} segments", current_level_idx);
        auto storage{m_levels.level(current_level_idx)->storage()};
        for (const auto &pSegment : *storage)
        {
            spdlog::debug("Segment name {}", pSegment->get_name());
            pSegment->restore();
        }

        current_level_idx++;
    }
    spdlog::info("In-memory index restoration finished");
    spdlog::info("Recovery finished");

    return true;
}

auto lsmtree_t::restore_wal() noexcept -> bool
{
    auto stringify_record = [](const memtable_t::record_t &record) -> std::string
    {
        std::stringstream strStream;
        record.write(strStream);
        return strStream.str();
    };

    const auto &records{m_pWal->records()};
    for (const auto &record : records)
    {
        switch (record.op)
        {
        case db::wal::wal_t::operation_k::add_k:
        {
            assert(m_table.has_value());
            spdlog::debug("Recovering record {} from WAL", stringify_record(record.kv));
            m_table->emplace(record.kv);
            break;
        }
        case db::wal::wal_t::operation_k::delete_k:
        {
            spdlog::debug("Recovery of delete records from WAS is not supported");
            break;
        }
        default:
        {
            spdlog::error("Unkown WAL operation {}", static_cast<std::int32_t>(record.op));
            break;
        }
        };
    }

    return true;
}

} // namespace structures::lsmtree
