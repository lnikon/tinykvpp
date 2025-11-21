#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <expected>
#include <memory>
#include <optional>
#include <cassert>
#include <utility>

#include <absl/synchronization/mutex.h>
#include <spdlog/spdlog.h>

#include "structures/lsmtree/segments/helpers.h"
#include "structures/lsmtree/segments/lsmtree_segment_factory.h"
#include "structures/memtable/memtable.h"
#include "structures/lsmtree/lsmtree_types.h"
#include "structures/lsmtree/lsmtree.h"
#include "db/manifest/manifest.h"
#include "tinykvpp/v1/tinykvpp_service.pb.h"

namespace structures::lsmtree
{

using level_operation_k = db::manifest::manifest_t::level_record_t::operation_k;
using segment_operation_k = db::manifest::manifest_t::segment_record_t::operation_k;

lsmtree_t::lsmtree_t(
    config::shared_ptr_t              pConfig,
    memtable::memtable_t              memtable,
    db::manifest::shared_ptr_t        pManifest,
    std::unique_ptr<levels::levels_t> pLevels
) noexcept
    : m_pConfig{pConfig},
      m_table{std::make_optional(memtable)},
      m_pManifest{std::move(pManifest)},
      m_pLevels{std::move(pLevels)},
      m_flushing_thread([this](std::stop_token stoken) { memtable_flush_task(stoken); })
{
}

lsmtree_t::~lsmtree_t() noexcept
{
    m_flushing_thread.request_stop();
    if (m_flushing_thread.joinable())
    {
        spdlog::debug("Waiting for flushing thread to finish");
        m_flushing_thread.join();
        spdlog::debug("Waiting for flushing thread to finish... Done");
    }
    else
    {
        spdlog::debug("Flushing thread is not joinable, skipping join");
    }
}

auto lsmtree_t::put(record_t record) noexcept -> std::expected<lsmtree_success_k, lsmtree_error_k>
{
    assert(m_pConfig);

    absl::WriterMutexLock lock{&m_mutex};
    assert(m_table);

    // TODO(lnikon): Can memtable emplace fail?
    m_table->emplace(std::move(record));

    // TODO(lnikon): Most probably this 'if' block will causes periodic latencies during reads
    if (m_table->size() >= m_pConfig->LSMTreeConfig.DiskFlushThresholdSize)
    {
        m_flushing_queue.push(std::move(m_table.value()));
        m_table = std::make_optional<memtable::memtable_t>();
        return lsmtree_success_k::memtable_reset_k;
    }

    return lsmtree_success_k::ok_k;
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

    // Lookup in immutable memtables
    if (!result.has_value())
    {
        spdlog::info(
            "cant find record in memtable... searching in flush queues size={}",
            m_flushing_queue.size()
        );
        result = m_flushing_queue.find<memtable::memtable_t::record_t>(key);
        if (result.has_value())
        {
            spdlog::info("found record in flushing queues");
        }
    }
    else
    {
        spdlog::info("found record in memtable");
    }

    // If key isn't in in-memory table, then it probably was flushed.
    // Lookup for the key in on-disk segments
    if (!result.has_value())
    {
        spdlog::info("cant find record in flush queues... searching in levels");
        result = m_pLevels->record(key);
        if (!result.has_value())
        {
            spdlog::info("cant find record in levels :(((");
        }
        else
        {
            spdlog::info("found record in levels");
        }
    }

    return result;
}

void lsmtree_t::memtable_flush_task(std::stop_token stoken) noexcept
{
    spdlog::info("Flushing thread started");

    // Continuously flush memtables to disk.
    // TODO: Is it possible to do the flushing async?
    while (true)
    {
        // Flush the remaining memtables on stop request and return
        if (stoken.stop_requested())
        {
            spdlog::debug(
                "Flushing remaining memtables on stop "
                "request. queue.size={}",
                m_flushing_queue.size()
            );

            // auto memtables = m_flushing_queue.pop_all();
            auto memtables = m_flushing_queue.reserve();
            if (!memtables.has_value())
            {
                return;
            }

            for (auto &memtable : memtables.value())
            {
                // TODO: Assert will crash the program, maybe we
                // should return an error code?
                absl::WriterMutexLock lock{&m_mutex};
                bool                  ok{m_pLevels->flush_to_level0(std::move(memtable))};
                assert(ok);
            }

            m_flushing_queue.consume();
            return;
        }

        auto memtables = m_flushing_queue.reserve();
        if (!memtables.has_value())
        {
            absl::SleepFor(absl::Milliseconds(50));
            continue;
        }

        auto idx{1};
        for (auto &memtable : memtables.value())
        {
            spdlog::debug("LSMTree: Flushing {}/{} memtable into disk", idx++, memtables->size());

            // TODO: Assert will crash the program, maybe we should
            // return an error code?
            absl::WriterMutexLock lock{&m_mutex};
            bool                  ok{m_pLevels->flush_to_level0(std::move(memtable))};
            // assert(ok);
        }
        m_flushing_queue.consume();
    }
}

auto lsmtree_builder_t::build(
    config::shared_ptr_t pConfig, db::manifest::shared_ptr_t pManifest, wal_t pWal
) const -> std::shared_ptr<lsmtree_t>
{
    auto memtable{build_memtable_from_wal(std::move(pWal))};
    if (!memtable.has_value())
    {
        spdlog::error("lsmtree_builder_t::build: Unable to build memtable.");
        return nullptr;
    }

    auto levels{build_levels_from_manifest(pConfig, pManifest)};
    if (!levels.has_value())
    {
        spdlog::error("lsmtree_builder_t::build: Unable to build levels.");
        return nullptr;
    }

    return std::make_shared<lsmtree_t>(
        std::move(pConfig),
        std::move(memtable.value()),
        std::move(pManifest),
        std::move(levels.value())
    );
}

auto lsmtree_builder_t::build_memtable_from_wal(wal_t pWal) noexcept
    -> std::optional<memtable::memtable_t>
{
    memtable::memtable_t table;
    for (const auto &records{pWal->records()}; const auto &record : records)
    {
        tinykvpp::v1::DatabaseOperation op;
        op.ParseFromString(record.payload());

        switch (op.type())
        {
        case tinykvpp::v1::DatabaseOperation::TYPE_PUT:
        {
            structures::memtable::memtable_t::record_t item;
            item.m_key.m_key = op.key();
            item.m_value.m_value = op.value();

            table.emplace(std::move(item));

            spdlog::debug("Recovered record {} from WAL", record.payload());

            break;
        }
        case tinykvpp::v1::DatabaseOperation::TYPE_DELETE:
        {
            spdlog::debug("Recovery of delete records from WAL is not supported");
            break;
        }
        default:
        {
            spdlog::error("Unknown WAL operation {}", std::to_underlying(op.type()));
            break;
        }
        };
    }

    return std::make_optional(std::move(table));
}

auto lsmtree_builder_t::build_levels_from_manifest(
    config::shared_ptr_t pConfig, db::manifest::shared_ptr_t pManifest
) noexcept -> std::optional<std::unique_ptr<levels::levels_t>>
{
    assert(pManifest);

    using level_operation_k = db::manifest::manifest_t::level_record_t::operation_k;
    using segment_operation_k = db::manifest::manifest_t::segment_record_t::operation_k;

    pManifest->disable();

    auto pLevels = std::make_unique<levels::levels_t>(pConfig, pManifest);

    const auto &records{pManifest->records()};
    for (const auto &record : records)
    {
        std::visit(
            [&](auto record) -> auto
            {
                using T = std::decay_t<decltype(record)>;
                if constexpr (std::is_same_v<T, db::manifest::manifest_t::segment_record_t>)
                {
                    switch (record.op)
                    {
                    case segment_operation_k::add_segment_k:
                    {
                        pLevels->level(record.level)
                            ->emplace(
                                segments::factories::lsmtree_segment_factory(
                                    record.name,
                                    segments::helpers::segment_path(
                                        pConfig->datadir_path(), record.name
                                    ),
                                    memtable_t{}
                                )
                            );

                        spdlog::debug(
                            "Segment {} added into level {} during recovery",
                            record.name,
                            record.level
                        );

                        break;
                    }
                    case segment_operation_k::remove_segment_k:
                    {
                        pLevels->level(record.level)->purge(record.name);
                        spdlog::debug(
                            "Segment {} removed from level {} during recovery",
                            record.name,
                            record.level
                        );

                        break;
                    }
                    default:
                    {
                        spdlog::error(
                            "Unknown segment operation={}", static_cast<std::int32_t>(record.op)
                        );

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
                        pLevels->level();
                        spdlog::debug("Level {} created during recovery", record.level);
                        break;
                    }
                    case level_operation_k::compact_level_k:
                    {
                        spdlog::debug(
                            "Ignoring {} during recovery",
                            db::manifest::manifest_t::level_record_t::ToString(
                                level_operation_k::compact_level_k
                            )
                        );
                        break;
                    }
                    case level_operation_k::purge_level_k:
                    {
                        spdlog::debug(
                            "Ignoring {} during recovery",
                            db::manifest::manifest_t::level_record_t::ToString(
                                level_operation_k::purge_level_k
                            )
                        );
                        break;
                    }
                    default:
                    {
                        spdlog::error(
                            "Unknown level operation={}", static_cast<std::int32_t>(record.op)
                        );
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
            record
        );
    }

    spdlog::debug("Restoring levels");
    pLevels->restore();
    spdlog::debug("Recovery finished");

    pManifest->enable();

    return std::make_optional<std::unique_ptr<levels::levels_t>>(std::move(pLevels));
}

} // namespace structures::lsmtree
