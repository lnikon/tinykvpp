#include "db/wal/wal.h"
#include "structures/memtable/memtable.h"
#include <absl/synchronization/mutex.h>
#include <db/manifest/manifest.h>
#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/helpers.h>
#include <structures/lsmtree/lsmtree.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>

#include <optional>
#include <stop_token>
#include <sstream>
#include <cassert>

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
      m_levels{pConfig, m_pManifest},
      m_recovered(false),
      m_flushing_thread(
          [this](std::stop_token stoken)
          {
              // Wait until LSMTree is recovered. Important during the DB opening phase.
              while (!m_recovered.load())
              {
                  if (stoken.stop_requested())
                  {
                      break;
                  }
              }

              spdlog::info("Flushing thread started");

              // Continuously flush memtables to disk
              // TODO: Is it possible to do the flushing async?
              while (true)
              {
                  if (stoken.stop_requested())
                  {
                      // Flush the remaining memtables on stop request
                      spdlog::debug("Flushing remaining memtables on stop request. queue.size={}",
                                    m_flushing_queue.size());

                      auto memtables = m_flushing_queue.pop_all();
                      while (!memtables.empty())
                      {
                          auto memtable = memtables.front();
                          memtables.pop_front();

                          // TODO: Assert will crash the program, maybe we should return an error code?
                          assert(m_levels.flush_to_level0(std::move(memtable)));
                      }
                      return;
                  }

                  if (std::optional<memtable::memtable_t> memtable = m_flushing_queue.pop();
                      memtable.has_value() && !memtable->empty())
                  {
                      spdlog::debug("Flushing memtable to level0. memtable.size={}, flushing_queue.size={}",
                                    memtable.value().size(),
                                    m_flushing_queue.size());

                      // TODO: Assert will crash the program, maybe we should return an error code?
                      absl::WriterMutexLock lock{&m_mutex};
                      assert(m_levels.flush_to_level0(std::move(memtable.value())));
                  }
              }
          })
{
}

lsmtree_t::~lsmtree_t() noexcept
{
    m_flushing_thread.request_stop();
    m_flushing_thread.join();
}

void lsmtree_t::put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value) noexcept
{
    assert(m_pConfig);

    absl::MutexLock lock{&m_mutex};
    assert(m_table);

    // Record addition of the new key into the WAL and add record into memtable
    auto record{record_t{key, value}};
    m_pWal->add({db::wal::wal_t::operation_k::add_k, record});
    m_table->emplace(std::move(record));

    // TODO: Most probably this if block will causes periodic latencies during reads when the condition is met
    if (m_table->size() >= m_pConfig->LSMTreeConfig.DiskFlushThresholdSize)
    {
        // Push the memtable to the flushing queue
        m_flushing_queue.push(std::move(m_table.value()));

        // Create a new memtable
        m_table = std::make_optional<memtable::memtable_t>();

        // Reset the Write-Ahead Log (WAL) to start logging anew
        m_pWal->reset();
    }
}

auto lsmtree_t::get(const key_t &key) noexcept -> std::optional<record_t>
{
    absl::MutexLock lock{&m_mutex};
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

    // Lookup in immutable memtables
    if (!result.has_value())
    {
        result = m_flushing_queue.find<memtable::memtable_t::record_t>(key);
    }

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
    if (!restore_from_manifest())
    {
        spdlog::error("Unable to restore manifest at {}", m_pManifest->path().c_str());
        return false;
    }

    // Restore memtable from WAL
    if (!restore_from_wal())
    {
        spdlog::error("Unable to restore WAL at {}", m_pWal->path().c_str());
        return false;
    }

    // Enable updates to manifest after recovery is finished
    m_pManifest->enable();

    // Signal that recovery is finished
    m_recovered.store(true);

    return true;
}

/**
 * ============================================================================
 * Private, utility functions
 * ============================================================================
 */

auto lsmtree_t::restore_from_manifest() noexcept -> bool
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
                        spdlog::debug("Segment {} removed from level {} during recovery", record.name, record.level);
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
                        spdlog::debug("Level {} created during recovery", record.level);
                        break;
                    }
                    case level_operation_k::compact_level_k:
                    {
                        spdlog::debug(
                            "Ignoring {} during recovery",
                            db::manifest::manifest_t::level_record_t::ToString(level_operation_k::compact_level_k));
                        break;
                    }
                    case level_operation_k::purge_level_k:
                    {
                        spdlog::debug(
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

    spdlog::debug("Restoring levels");
    m_levels.restore();
    spdlog::debug("Recovery finished");

    return true;
}

auto lsmtree_t::restore_from_wal() noexcept -> bool
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
