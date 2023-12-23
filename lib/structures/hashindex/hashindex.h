#pragma once

#include <cstdint>       // std::uint64_t
#include <unordered_map> // std::unordered_map

#include <structures/lsmtree/lsmtree_types.h>

namespace structures::hashindex {

// TODO(lnikon): Benchmarks with flat_*maps to understand the
// performance.
class hashindex_t {
public:
  using offset_t = std::uint64_t;
  using cursor_t = std::uint64_t;

  hashindex_t();
  hashindex_t(const hashindex_t &) = default;
  hashindex_t(hashindex_t &&) = default;
  hashindex_t &operator=(const hashindex_t &) = default;
  hashindex_t &operator=(hashindex_t &&) = default;

  void emplace(structures::lsmtree::key_t key);

private:
  cursor_t m_cursor;
  std::unordered_map<structures::lsmtree::key_t, offset_t> m_offsets;
};

} // namespace structures::hashindex
