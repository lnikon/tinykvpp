//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREEREGULARSEGMENT_H
#define ZKV_LSMTREEREGULARSEGMENT_H

#include "ILSMTreeSegment.h"

namespace structures::lsmtree {
    class LSMTreeRegularSegment : public ILSMTreeSegment {
    public:
        explicit LSMTreeRegularSegment(std::string name);

        void Flush() override;
    };
}

#endif //ZKV_LSMTREEREGULARSEGMENT_H
