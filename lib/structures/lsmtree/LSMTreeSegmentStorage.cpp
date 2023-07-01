#include "LSMTreeSegmentStorage.h"

namespace structures::lsmtree {

LSMTreeSegmentPtr LSMTreeSegmentStorage::Get(const NameType &name) const {
    assert(!name.empty());
    return nullptr;
}

void LSMTreeSegmentStorage::Put(LSMTreeSegmentPtr pLsmTreeSegment) {
    assert(pLsmTreeSegment);
}

void LSMTreeSegmentStorage::Remove(const NameType &name) {
    assert(!name.empty());
}

} // namespace structures::lsmtree
