#pragma once

#include <memory>
#include <optional>
#include <sys/types.h>

#include <spdlog/spdlog.h>
#include <absl/synchronization/mutex.h>

#include "db/manifest/manifest.h"
#include "wal/wal.h"
#include "structures/lsmtree/levels/levels.h"
#include "concurrency/thread_safe_queue.h"

namespace structures::lsmtree
{

enum class lsmtree_status_k : uint8_t
{
    ok_k,
    memtable_reset_k,
    put_failed_k,
    get_failed_k,
    recover_failed_k,
    restore_manifest_failed_k,
    restore_wal_failed_k,
    flush_failed_k,
    segment_add_failed_k,
    segment_remove_failed_k,
    segment_compact_failed_k,
    segment_purge_failed_k,
    level_add_failed_k,
    level_compact_failed_k,
    level_purge_failed_k,
    unknown_error_k
};

class lsmtree_t final
{
  public:
    using record_t = structures::memtable::memtable_t::record_t;
    using key_t = record_t::key_t;

    explicit lsmtree_t(
        config::shared_ptr_t              pConfig,
        memtable::memtable_t              memtable,
        db::manifest::shared_ptr_t        pManifest,
        std::unique_ptr<levels::levels_t> pLevels
    ) noexcept;

    lsmtree_t() = delete;

    lsmtree_t(const lsmtree_t &) = delete;
    auto operator=(const lsmtree_t &) -> lsmtree_t & = delete;

    lsmtree_t(lsmtree_t &&other) noexcept;
    auto operator=(lsmtree_t &&other) noexcept -> lsmtree_t &;

    ~lsmtree_t() noexcept;

    [[nodiscard]] auto put(record_t record) noexcept -> lsmtree_status_k;
    [[nodiscard]] auto get(const key_t &key) noexcept -> std::optional<record_t>;

  private:
    void memtable_flush_task(std::stop_token stoken) noexcept;
    void move_from(lsmtree_t &&other) noexcept;

    config::shared_ptr_t m_pConfig;

    // TODO(lnikon): Add absl:: thread guards!
    absl::Mutex                                            m_mutex;
    std::optional<memtable::memtable_t> m_table            ABSL_GUARDED_BY(m_mutex);
    db::manifest::shared_ptr_t m_pManifest                 ABSL_GUARDED_BY(m_mutex);
    std::unique_ptr<levels::levels_t> m_pLevels            ABSL_GUARDED_BY(m_mutex);
    std::jthread                                           m_flushing_thread;
    concurrency::thread_safe_queue_t<memtable::memtable_t> m_flushing_queue;
};

using shared_ptr_t = std::shared_ptr<lsmtree_t>;

struct lsmtree_builder_t final
{
    [[nodiscard]] auto build(
        config::shared_ptr_t                pConfig,
        db::manifest::shared_ptr_t          pManifest,
        wal::shared_ptr_t<wal::wal_entry_t> pWal
    ) const -> std::shared_ptr<lsmtree_t>;

  private:
    [[nodiscard]] auto
    build_memtable_from_wal(wal::shared_ptr_t<wal::wal_entry_t> pWal) const noexcept
        -> std::optional<memtable::memtable_t>;

    [[nodiscard]] auto build_levels_from_manifest(
        config::shared_ptr_t pConfig, db::manifest::shared_ptr_t pManifest
    ) const noexcept -> std::optional<std::unique_ptr<levels::levels_t>>;
};

} // namespace structures::lsmtree
