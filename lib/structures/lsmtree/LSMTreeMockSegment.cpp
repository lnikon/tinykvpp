//
// Created by nikon on 2/6/22.
//

#include "LSMTreeMockSegment.h"

namespace structures::lsmtree {
    LSMTreeMockSegment::LSMTreeMockSegment(std::string name)
        : ILSMTreeSegment(std::move(name)) {}

    void LSMTreeMockSegment::Flush() {
        assert(!m_content.empty());
        spdlog::info("mock segment flush");
        spdlog::info("going to flash {:s}", m_content);
    }
}
