#ifndef LSM_TREE_SEGMENT_STORAGE
#define LSM_TREE_SEGMENT_STORAGE

#include "ILSMTreeSegment.h"

#include <string>
#include <unordered_map>

namespace structures::lsmtree {

class LSMTreeSegmentStorage {
public:
  using NameType = std::string;

  LSMTreeSegmentPtr Get(const NameType &name) const;
  void Put(LSMTreeSegmentPtr pLsmTreeSegment);
  void Remove(const NameType &name);

private:
  std::unordered_map<NameType, LSMTreeSegmentPtr> m_segments;
};

} // namespace structures::lsmtree

#endif // LSM_TREE_SEGMENT_STORAGE
