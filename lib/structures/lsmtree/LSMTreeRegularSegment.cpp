//
// Created by nikon on 2/6/22.
//

#include "LSMTreeRegularSegment.h"

namespace structures::lsmtree {
    LSMTreeRegularSegment::LSMTreeRegularSegment(std::string name)
        : ILSMTreeSegment(std::move(name)) {}

    void LSMTreeRegularSegment::Flush() {
        assert(!m_content.empty());
        spdlog::info("regular segment flush");
        spdlog::info("going to flash {:s}", m_content);
    }
}
