//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREEMOCKSEGMENT_H
#define ZKV_LSMTREEMOCKSEGMENT_H

#include <filesystem>

#include <structures/lsmtree/segments/interface_lsmtree_segment.h>

namespace structures::lsmtree {

class lsmtree_mock_segment_t : public interface_lsmtree_segment_t {
public:
  lsmtree_mock_segment_t(std::filesystem::path path, memtable_unique_ptr_t pMemtable);

  void flush() override;
};

} // namespace structures::lsmtree

#endif // ZKV_LSMTREEMOCKSEGMENT_H