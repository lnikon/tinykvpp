#pragma once

#include <map>
#include <cstdint>

#include "structures/memtable/memtable.h"

namespace structures::hashindex
{

// TODO(lnikon): Benchmark with flat_*maps to understand the
// performance.
class hashindex_t final
{
  public:
    using key_t = structures::memtable::memtable_t::record_t::key_t;
    using record_t = structures::memtable::memtable_t::record_t;
    using offset_t = std::uint64_t;
    using storage_t = std::multimap<key_t, offset_t>;
    using iterator = storage_t::iterator;

    [[nodiscard]] auto begin() -> iterator;
    [[nodiscard]] auto end() -> iterator;

    void               emplace(record_t record, std::size_t length);
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto offset(const key_t &key) const -> std::vector<offset_t>;

    [[nodiscard]] auto num_of_bytes_used() const noexcept -> std::size_t;

  private:
    storage_t   m_offsets;
    std::size_t m_num_of_bytes{0};
};

} // namespace structures::hashindex
