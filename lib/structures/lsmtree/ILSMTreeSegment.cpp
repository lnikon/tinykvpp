//
// Created by nikon on 2/6/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include "ILSMTreeSegment.h"

namespace structures::lsmtree {
    ILSMTreeSegment::ILSMTreeSegment(std::string name)
        : m_name(std::move(name)) {}

    void ILSMTreeSegment::SetContent(std::string content) {
        m_content = std::move(content);
        spdlog::debug("content={}", m_content);;
    }

    std::string ILSMTreeSegment::GetName() const {
        return m_name;
    }
}
