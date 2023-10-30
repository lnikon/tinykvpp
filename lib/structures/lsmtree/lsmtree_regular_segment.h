//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREEREGULARSEGMENT_H
#define ZKV_LSMTREEREGULARSEGMENT_H

#include "interface_lsmtree_segment.h"

namespace structures::lsmtree {

class lsmtree_regular_segment_t : public interface_lsmtree_segment_t {
public:
  explicit lsmtree_regular_segment_t(std::string name,
                                     memtable_unique_ptr_t pMemtable);

  void flush() override;
};

} // namespace structures::lsmtree

#endif // ZKV_LSMTREEREGULARSEGMENT_H
