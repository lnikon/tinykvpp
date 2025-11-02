#pragma once

#include <cstdint>

#include <absl/synchronization/mutex.h>

#include "structures/lsmtree/segments/lsmtree_regular_segment.h"
#include "db/manifest/manifest.h"
#include "config/config.h"
#include "structures/lsmtree/segments/segment_storage.h"

namespace structures::lsmtree::level
{

class level_t
{
  public:
    using level_index_type_t = std::uint64_t;
    using record_t = structures::memtable::memtable_t::record_t;
    using key_t = record_t::key_t;

    level_t() = delete;

    level_t(
        level_index_type_t         levelIndex,
        config::shared_ptr_t       pConfig,
        db::manifest::shared_ptr_t manifest
    ) noexcept;

    level_t(const level_t &) = delete;
    auto operator=(const level_t &) -> level_t & = delete;

    level_t(level_t &&) = delete;
    auto operator=(level_t &&) -> level_t & = delete;

    ~level_t() noexcept = default;

    void emplace(const segments::regular_segment::shared_ptr_t &pSegment) noexcept;

    [[maybe_unused]] auto segment(memtable::memtable_t pMemtable)
        -> segments::regular_segment::shared_ptr_t;

    auto segment(memtable::memtable_t memtable, const std::string &name)
        -> segments::regular_segment::shared_ptr_t;

    [[nodiscard]] auto record(const key_t &key) const noexcept
        -> std::optional<memtable::memtable_t::record_t>;

    [[nodiscard]] auto compact() const noexcept -> segments::regular_segment::shared_ptr_t;

    void merge(const segments::regular_segment::shared_ptr_t &pSegment) noexcept;

    void purge() noexcept;

    void purge(const segments::types::name_t &segmentName) noexcept;

    auto restore() noexcept -> void;

    [[nodiscard]] auto index() const noexcept -> level_index_type_t;

    [[__nodiscard__]] auto bytes_used() const noexcept -> std::size_t;

  private:
    void purge(segments::regular_segment::shared_ptr_t pSegment) noexcept;

    mutable absl::Mutex m_mutex;

    const level_index_type_t             m_levelIndex;
    config::shared_ptr_t                 m_pConfig;
    segments::storage::segment_storage_t m_storage;
    db::manifest::shared_ptr_t           m_manifest;
};

using shared_ptr_t = std::shared_ptr<level_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<level_t>(std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::level
