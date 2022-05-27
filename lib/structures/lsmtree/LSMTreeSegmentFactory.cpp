//
// Created by nikon on 2/6/22.
//

#include "LSMTreeSegmentFactory.h"

namespace structures::lsmtree {
    LSMTreeSegmentPtr LSMTreeSegmentFactory(const LSMTreeSegmentType type, std::string name) {
        switch (type) {
            case LSMTreeSegmentType::Mock:
                return std::make_shared<LSMTreeMockSegment>(name);
            case LSMTreeSegmentType::Regular:
                return std::make_shared<LSMTreeRegularSegment>(name);
            default:
                spdlog::error("unhandled lsm tree segment type");
                assert(false);
        }
    }
}
