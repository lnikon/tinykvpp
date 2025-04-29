#pragma once

#include "structures/lsmtree/segments/types.h"
#include "structures/memtable/memtable.h"
#include "structures/hashindex/hashindex.h"
#include "fs/types.h"

namespace structures::lsmtree::segments::regular_segment
{

namespace types = lsmtree::segments::types;

class regular_segment_t final
{
  public:
    regular_segment_t(fs::path_t path, types::name_t name, memtable::memtable_t memtable) noexcept;

    regular_segment_t(const regular_segment_t &) = delete;
    auto operator=(const regular_segment_t &) -> regular_segment_t & = delete;

    regular_segment_t(regular_segment_t &&) = delete;
    auto operator=(regular_segment_t &&) -> regular_segment_t & = delete;

    ~regular_segment_t() = default;

    [[nodiscard]] auto record(const lsmtree::key_t &key)
        -> std::vector<std::optional<memtable::memtable_t::record_t>>;

    [[nodiscard]] auto record(const hashindex::hashindex_t::offset_t &offset)
        -> std::optional<memtable::memtable_t::record_t>;

    [[nodiscard]] auto get_name() const -> types::name_t;
    [[nodiscard]] auto get_path() const -> types::path_t;

    auto memtable() -> std::optional<memtable::memtable_t> &;
    auto moved_memtable() -> std::optional<memtable::memtable_t>;

    void restore();
    void flush();
    void remove_from_disk() const noexcept;
    auto last_write_time() -> std::filesystem::file_time_type;

    [[nodiscard]] auto min() const noexcept -> std::optional<memtable::memtable_t::record_t::key_t>;
    [[nodiscard]] auto max() const noexcept -> std::optional<memtable::memtable_t::record_t::key_t>;

    [[nodiscard]] auto num_of_bytes_used() const -> std::size_t;

  private:
    void restore_index();

    const fs::path_t    m_path;
    const types::name_t m_name;

    hashindex::hashindex_t              m_hashIndex;
    std::optional<memtable::memtable_t> m_memtable;

    std::optional<memtable::memtable_t::record_t::key_t> m_minKey;
    std::optional<memtable::memtable_t::record_t::key_t> m_maxKey;
};

using shared_ptr_t = std::shared_ptr<regular_segment_t>;

template <typename... Args> auto make_shared(Args... args)
{
    return std::make_shared<regular_segment_t>(std::forward<Args>(args)...);
}

} // namespace structures::lsmtree::segments::regular_segment
