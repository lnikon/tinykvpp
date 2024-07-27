#include "db/wal/wal.h"
#include <db/manifest/manifest.h>
#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/helpers.h>
#include <structures/lsmtree/lsmtree.h>
#include <structures/lsmtree/segments/segment_interface.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>

#include <cstdint>
#include <optional>
#include <type_traits>

#include <spdlog/spdlog.h>

namespace structures::lsmtree
{

using level_operation_k = db::manifest::manifest_t::level_record_t::operation_k;
using segment_operation_k = db::manifest::manifest_t::segment_record_t::operation_k;

lsmtree_t::lsmtree_t(const config::shared_ptr_t pConfig,
                     db::manifest::shared_ptr_t manifest,
                     db::wal::shared_ptr_t wal) noexcept
    : m_pConfig{pConfig},
      m_table{std::make_optional<memtable::memtable_t>()},
      m_manifest{manifest},
      m_wal{wal},
      m_levels{pConfig, m_manifest}
{
}

void lsmtree_t::put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value) noexcept
{
    assert(m_table);
    assert(m_pConfig);

    // Record addition of the new key into the WAL and add record into memtable
    auto record{record_t{key, value}};
    m_wal->add(db::wal::wal_t::operation_k::add_k, record);
    m_table->emplace(record);

    // Check whether after addition size of the memtable increased above the
    // threashold. If so flush the memtable
    // TODO(lnikon): This logic should be a part of
    if (m_table->size() >= m_pConfig->LSMTreeConfig.DiskFlushThresholdSize)
    {
        m_levels.segment(m_pConfig->LSMTreeConfig.SegmentType, std::move(m_table.value()));
        m_table = std::make_optional<memtable::memtable_t>();
        m_wal->reset();
    }
}

std::optional<record_t> lsmtree_t::get(const key_t &key) noexcept
{
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

bool lsmtree_t::restore() noexcept
{
    assert(m_pConfig);
    assert(m_manifest);

    // Disable all updates to manifest in recovery phase
    // because nothing new is happening, lsmtree is re-using already
    // existing information
    m_manifest->disable();

    // Restore lsmtree structure from the manifest file
    restore_manifest();

    // Restore memtable from WAL
    restore_wal();

    // Enable updates to manifest after recovery is finished
    m_manifest->enable();

    return false;
}

bool lsmtree_t::restore_manifest() noexcept
{
    const auto &records{m_manifest->records()};
    for (const auto &record : records)
    {
        std::visit(
            [this](auto &&record)
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
                                lsmtree_segment_type_t::regular_k,
                                record.name,
                                segments::helpers::segment_path(m_pConfig->datadir_path(), record.name),
                                std::move(memtable_t{})));
                        spdlog::info("add segment {} into level {} during recovery", record.name, record.level);
                        break;
                    }
                    case segment_operation_k::remove_segment_k:
                    {
                        m_levels.level(record.level)->purge(record.name);
                        spdlog::info("remove segment {} from level {} during recovery", record.name, record.level);
                        break;
                    }
                    default:
                    {
                        spdlog::error("unkown segment operation={}", static_cast<std::int32_t>(record.op));
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
                        spdlog::info("create level {} during recovery", record.level);
                        break;
                    }
                    case level_operation_k::compact_level_k:
                    {
                        spdlog::info("ignoring compact_level_k during recovery");
                        break;
                    }
                    case level_operation_k::purge_level_k:
                    {
                        spdlog::info("ignoring purge_level_k during recovery");
                        break;
                    }
                    default:
                    {
                        spdlog::error("unkown level operation={}", static_cast<std::int32_t>(record.op));
                        break;
                    }
                    }
                }
                else
                {
                    spdlog::error("unkown manifest record type");
                }
            },
            record);
    }

    // A little bit of printbugging :)
    // Now is that all modifications are applied to the segments storage it is time to restore segments.
    // This will repopulate indices
    spdlog::info("recovery finished");
    const auto level_count{m_levels.size()};
    std::size_t current_level_idx{0};
    while (current_level_idx != level_count)
    {
        spdlog::info("level {} has following segments...", current_level_idx);
        auto storage{m_levels.level(current_level_idx)->storage()};
        for (auto pSegment : *storage)
        {
            pSegment->restore();
            spdlog::info("segment {}", pSegment->get_name());
        }

        current_level_idx++;
    }

    return true;
}

bool lsmtree_t::restore_wal() noexcept
{
    const auto &records{m_wal->records()};
    for (const auto &record : records)
    {
        switch (record.op)
        {
        case db::wal::wal_t::operation_k::add_k:
        {
            spdlog::info("add_k kv into memtable during WAL recovery");
            m_table->emplace(record.kv);
            break;
        }
        case db::wal::wal_t::operation_k::delete_k:
        {
            spdlog::info("delete_k is not supported yet in WAL recovery");
            break;
        }
        default:
        {
            spdlog::error("unkown WAL operation {}", static_cast<std::int32_t>(record.op));
            break;
        }
        };
    }

    return true;
}

} // namespace structures::lsmtree
