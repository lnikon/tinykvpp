#pragma once

#include <unordered_map>

namespace structures::lsmtree
{

/**
 * Solely in-memory data structures.
 * Should be populated every time when DB wakes up.
 * Maps keys to their key-value pair offset relative to disk segment in which
 * that pair is stored.
 */
class lsmtree_segment_index_t
{
  public:
    using offset_type_t = std::size_t;

    lsmtree_segment_index_t(const lsmtree_segment_index_t &) = delete;
    lsmtree_segment_index_t &operator=(const lsmtree_segment_index_t &) = delete;
    lsmtree_segment_index_t(lsmtree_segment_index_t &&) = delete;
    lsmtree_segment_index_t &operator=(lsmtree_segment_index_t &&) = delete;

    struct key_t
    {
    };

    // TODO: Decide on the interface.
    offset_type_t write(const key_t &key);
    offset_type_t read(const key_t &key) const;

  private:
    offset_type_t                            m_lastOffset{0};
    std::unordered_map<key_t, offset_type_t> m_index;
};

} // namespace structures::lsmtree
