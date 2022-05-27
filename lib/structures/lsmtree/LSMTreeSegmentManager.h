//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREESEGMENTMANAGER_H
#define ZKV_LSMTREESEGMENTMANAGER_H

#include <unordered_map>

#include "ILSMTreeSegment.h"
#include "LSMTreeTypes.h"
#include "LSMTreeSegmentFactory.h"

namespace structures::lsmtree {
    /**
     * Does the management of on-disk segments.
     * On-disk segment is born when memtable is flushed onto the disk.
     */
    class LSMTreeSegmentManager {
    public:
        /**
         * TODO: Should be thread-safe?
         */
        LSMTreeSegmentPtr GetNewSegment(const LSMTreeSegmentType type);
        LSMTreeSegmentPtr GetSegment(const std::string &name);

        // TODO: Start merging on-disk segments.
        // void Compact();
    private:
        std::string getNextName();

    private:
        uint64_t m_index{0};
        std::unordered_map<std::string, LSMTreeSegmentPtr> m_segments;
    };

    using LSMTreeSegmentManagerPtr = std::shared_ptr<LSMTreeSegmentManager>;
}

#endif //ZKV_LSMTREESEGMENTMANAGER_H
