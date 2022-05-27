//
// Created by nikon on 1/26/22.
//

#ifndef ZKV_LSMTREECOMPACTOR_H
#define ZKV_LSMTREECOMPACTOR_H

#include <structures/lsmtree/LSMTreeReaderWriter.h>

namespace structures::lsmtree
{
//    class LSMTreeCompactor {
//    public:
//        void
//    };

void compactLSMTree(MemTableUniquePtr memTable);

}

#endif //ZKV_LSMTREECOMPACTOR_H
