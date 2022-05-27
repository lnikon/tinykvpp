//
// Created by nikon on 2/6/22.
//

#include "LSMTreeSegmentManager.h"

namespace structures::lsmtree {
    LSMTreeSegmentPtr LSMTreeSegmentManager::GetNewSegment(const structures::lsmtree::LSMTreeSegmentType type) {
        const auto name{getNextName()};
        auto result = LSMTreeSegmentFactory(type, name);
        m_segments[name] = result;
        return result;
    }

    LSMTreeSegmentPtr LSMTreeSegmentManager::GetSegment(const std::string &name) {
        assert(!name.empty());
        LSMTreeSegmentPtr result{nullptr};
        if (auto it = m_segments.find(name); it != m_segments.end()) {
            result = it->second;
        } else {
            spdlog::warn("unable to find lsm tree segment with name {:s}", name);
        }

        return result;
    }

    std::string LSMTreeSegmentManager::getNextName() {
        return "segment_" + std::to_string(m_index++);
    }
}