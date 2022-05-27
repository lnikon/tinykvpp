//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREEMOCKSEGMENT_H
#define ZKV_LSMTREEMOCKSEGMENT_H

#include "ILSMTreeSegment.h"

namespace structures::lsmtree {
    class LSMTreeMockSegment : public ILSMTreeSegment {
    public:
        LSMTreeMockSegment(std::string name);

        void Flush() override;
    };
}

#endif //ZKV_LSMTREEMOCKSEGMENT_H
