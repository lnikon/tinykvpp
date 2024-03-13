#pragma once

#include <structures/lsmtree/lsmtree_types.h>

#include <cstdint>        // std::uint64_t
#include <unordered_map>  // std::unordered_map

namespace structures::hashindex
{

// TODO(lnikon): Benchmarks with flat_*maps to understand the
// performance.
class hashindex_t
{
   public:
    using offset_t = std::uint64_t;
    using cursor_t = std::uint64_t;
    using storage_t = std::unordered_map<structures::lsmtree::key_t, offset_t>;
    using iterator = storage_t::iterator;

    hashindex_t();
    hashindex_t(const hashindex_t &) = default;
    hashindex_t(hashindex_t &&) = default;
    hashindex_t &operator=(const hashindex_t &) = default;
    hashindex_t &operator=(hashindex_t &&) = default;

    [[nodiscard]] iterator begin();
    [[nodiscard]] iterator end();

    void emplace(structures::lsmtree::key_t key);
    bool empty() const;

    [[nodiscard]] std::optional<offset_t> get_offset(
        const structures::lsmtree::key_t &key) const;

   private:
    cursor_t m_cursor;
    storage_t m_offsets;
};

}  // namespace structures::hashindex
