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
    using storage_t = std::multimap<structures::lsmtree::key_t, offset_t>;
    using iterator = storage_t::iterator;

    [[nodiscard]] auto begin() -> iterator;
    [[nodiscard]] auto end() -> iterator;

    void emplace(structures::lsmtree::record_t key, std::size_t length);
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto offset(const structures::lsmtree::key_t &key) const -> std::vector<offset_t>;

  private:
    storage_t m_offsets;
};

} // namespace structures::hashindex
