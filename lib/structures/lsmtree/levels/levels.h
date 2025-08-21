#pragma once

#include <optional>
#include <vector>

#include <absl/synchronization/mutex.h>
#include <absl/synchronization/notification.h>

#include "db/manifest/manifest.h"
#include "structures/lsmtree/levels/level.h"
#include "config/config.h"

namespace structures::lsmtree::levels
{

class levels_t
{
  public:
    using levels_storage_t = std::vector<structures::lsmtree::level::shared_ptr_t>;
    using record_t = structures::memtable::memtable_t::record_t;
    using key_t = record_t::key_t;

    explicit levels_t(config::shared_ptr_t pConfig, db::manifest::shared_ptr_t pManifest) noexcept;

    levels_t(const levels_t &) = delete;
    auto operator=(const levels_t &) -> levels_t & = delete;

    levels_t(levels_t &&other) noexcept;
    auto operator=(levels_t &&other) noexcept -> levels_t &;

    ~levels_t() noexcept;

    [[nodiscard]] auto record(const key_t &key) const noexcept -> std::optional<record_t>;
    [[nodiscard]] auto compact() -> segments::regular_segment::shared_ptr_t;
    [[nodiscard]] auto level() noexcept -> level::shared_ptr_t;
    [[nodiscard]] auto level(std::size_t idx) noexcept -> level::shared_ptr_t;

    [[nodiscard]] auto size() const noexcept -> levels_storage_t::size_type;

    [[nodiscard]] auto flush_to_level0(memtable::memtable_t memtable) const noexcept
        -> segments::regular_segment::shared_ptr_t;

    auto restore() noexcept -> void;

  private:
    void move_from(levels_t &&other) noexcept;
    void compaction_task(std::stop_token stoken) noexcept;

    config::shared_ptr_t m_pConfig;

    mutable absl::Mutex        m_mutex;
    db::manifest::shared_ptr_t m_pManifest;
    levels_storage_t           m_levels;

    mutable absl::Notification m_level0_segment_flushed_notification;
    std::jthread               m_compaction_thread;
};
} // namespace structures::lsmtree::levels
