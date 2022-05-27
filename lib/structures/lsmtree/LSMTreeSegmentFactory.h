//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREESEGMENTFACTORY_H
#define ZKV_LSMTREESEGMENTFACTORY_H

#include "ILSMTreeSegment.h"
#include "LSMTreeRegularSegment.h"
#include "LSMTreeMockSegment.h"

namespace structures::lsmtree {
    LSMTreeSegmentPtr LSMTreeSegmentFactory(const LSMTreeSegmentType type, std::string name);
}

#endif //ZKV_LSMTREESEGMENTFACTORY_H
