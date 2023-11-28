//
// Created by nikon on 1/21/22.
//

#ifndef CPP_PROJECT_TEMPLATE_LSMTREE_H
#define CPP_PROJECT_TEMPLATE_LSMTREE_H

#include <optional>
#include <thread>

#include <structures/lsmtree/lsmtree_config.h>
#include <structures/lsmtree/segments/lsmtree_segment_manager.h>
#include <structures/lsmtree/lsmtree_types.h>

namespace structures::lsmtree {

using namespace segment_manager;

/**
 * Encapsulates MemTable, SegmentManager, and SegmentIndices.
 */
class lsmtree_t {
public:
  // TODO: Make LSMTreeConfig configurable via CLI
  explicit lsmtree_t(const lsmtree_config_t &config,
                     lsmtree_segment_manager_shared_ptr_t pSegmentsMgr);

  lsmtree_t() = default;
  lsmtree_t(const lsmtree_t &) = delete;
  lsmtree_t &operator=(const lsmtree_t &) = delete;
  lsmtree_t(lsmtree_t &&) = delete;
  lsmtree_t &operator=(lsmtree_t &&) = delete;

  void put(const key_t &key, const value_t &value);
  std::optional<record_t> get(const key_t &key) const;

private:
  std::mutex m_mutex;
  lsmtree_config_t m_config;
  memtable_unique_ptr_t m_table;
  lsmtree_segment_manager_shared_ptr_t m_pSegmentsMgr;
  std::size_t m_size;
  // TODO(lnikon): Keep BloomFilter(BF) for reads. First check BF, if it says no, then
  // abort searching. Otherwise perform search.
  // TODO(lnikon): Keep in-memory indices for segments.
};

} // namespace structures::lsmtree

#endif // CPP_PROJECT_TEMPLATE_LSMTREE_H
