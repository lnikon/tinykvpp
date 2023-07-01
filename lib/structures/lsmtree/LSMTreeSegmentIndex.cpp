#include "LSMTreeSegmentIndex.h"

namespace structures::lsmtree {

LSMTreeSegmentIndex::OffsetType LSMTreeSegmentIndex::Write(const Key &key) {
    return m_lastOffset;
}

LSMTreeSegmentIndex::OffsetType LSMTreeSegmentIndex::Read(const Key &key) const {
    return m_lastOffset;
}

} // namespace structures::lsmtree
