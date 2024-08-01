#pragma once

#include <structures/lsmtree/lsmtree_types.h>

#include <cstdint>
#include <map>

namespace structures::hashindex
{

// TODO(lnikon): Benchmark with flat_*maps to understand the
// performance.
class hashindex_t
{
  public:
    using offset_t = std::uint64_t;
    using cursor_t = std::uint64_t;
    using storage_t = std::multimap<structures::lsmtree::key_t, offset_t>;
    using iterator = storage_t::iterator;

    hashindex_t();
    hashindex_t(const hashindex_t &) = default;
    hashindex_t(hashindex_t &&) = default;
    auto operator=(const hashindex_t &) -> hashindex_t & = default;
    auto operator=(hashindex_t &&) -> hashindex_t & = default;

    [[nodiscard]] auto begin() -> iterator;
    [[nodiscard]] auto end() -> iterator;

    void emplace(structures::lsmtree::record_t key, std::size_t length);
    [[nodiscard]] auto empty() const -> bool;

    [[nodiscard]] auto offset(const structures::lsmtree::key_t &key) const -> std::vector<offset_t>;

  private:
    cursor_t m_cursor;
    storage_t m_offsets;
};

} // namespace structures::hashindex
