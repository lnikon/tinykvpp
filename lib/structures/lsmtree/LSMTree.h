//
// Created by nikon on 1/21/22.
//

#ifndef CPP_PROJECT_TEMPLATE_LSMTREE_H
#define CPP_PROJECT_TEMPLATE_LSMTREE_H

#include <sstream>
#include <thread>

#include "ILSMTreeSegment.h"
#include "LSMTreeConfig.h"
#include "LSMTreeRegularSegment.h"
#include "LSMTreeSegmentStorage.h"
#include "LSMTreeSegmentFactory.h"
#include "LSMTreeSegmentManager.h"
#include "LSMTreeTypes.h"

namespace structures::lsmtree {

/**
 * Incapsulates MemTable, SegmentManager, and SegmentIndices.
 */
class LSMTree {
public:
  // TODO: Make LSMTreeConfig configurable via CLI
  explicit LSMTree(const LSMTreeConfig &config);

  LSMTree() = default;
  LSMTree(const LSMTree &) = delete;
  LSMTree &operator=(const LSMTree &) = delete;
  LSMTree(LSMTree &&) = delete;
  LSMTree &operator=(LSMTree &&) = delete;

  void Put(const Key &key, const Value &value);
  std::optional<Record> Get(const Key &key) const;

private:
  std::mutex m_mutex;
  LSMTreeConfig m_config;
  MemTableUniquePtr m_table;
  LSMTreeSegmentManagerPtr m_segmentsMgr;
  std::size_t m_size;
  // TODO: Keep BloomFilter(BF) for reads. First check BF, if it says no, then
  // abort searching. Otherwise perform search.
  // TODO: Keep in-memory indices for segments.
};

} // namespace structures::lsmtree

#endif // CPP_PROJECT_TEMPLATE_LSMTREE_H
