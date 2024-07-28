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
    hashindex_t &operator=(const hashindex_t &) = default;
    hashindex_t &operator=(hashindex_t &&) = default;

    [[nodiscard]] iterator begin();
    [[nodiscard]] iterator end();

    void emplace(structures::lsmtree::record_t key, const std::size_t length);
    bool empty() const;

    [[nodiscard]] std::vector<offset_t> offset(const structures::lsmtree::key_t &key) const;

  private:
    cursor_t m_cursor;
    storage_t m_offsets;
};

} // namespace structures::hashindex
