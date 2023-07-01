#ifndef LSM_TREE_SEGMENT_INDEX
#define LSM_TREE_SEGMENT_INDEX

#include "ILSMTreeSegment.h"

namespace structures::lsmtree {

/**
 * Solely in-memory data structures.
 * Should be populated every time when DB wakes up.
 * Maps keys to their key-value pair offset relative to disk segment in which
 * that pair is stored.
 */
class LSMTreeSegmentIndex {
    public:
        using OffsetType = std::size_t;

        LSMTreeSegmentIndex(const LSMTreeSegmentIndex&) = delete;
        LSMTreeSegmentIndex& operator=(const LSMTreeSegmentIndex&) = delete;
        LSMTreeSegmentIndex(LSMTreeSegmentIndex&&) = delete;
        LSMTreeSegmentIndex& operator=(LSMTreeSegmentIndex&&) = delete;

        // TODO: Decide on the interface.
        OffsetType Write(const Key& key);
        OffsetType Read(const Key& key) const;

    private:
        OffsetType m_lastOffset{0}; 
        std::unordered_map<Key, OffsetType> m_index;
};

}

#endif // LSM_TREE_SEGMENT_INDEX
