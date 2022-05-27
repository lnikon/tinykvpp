//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_ILSMTREESEGMENT_H
#define ZKV_ILSMTREESEGMENT_H

#include "LSMTreeTypes.h"

namespace structures::lsmtree {
    class ILSMTreeSegment {
    public:
        explicit ILSMTreeSegment(std::string name);

        void SetContent(std::string content);

        [[nodiscard]] std::string GetName() const;

        virtual void Flush() = 0;

    protected:
        std::string m_content;

    private:
        const std::string m_name;
    };

    using LSMTreeSegmentPtr = std::shared_ptr<ILSMTreeSegment>;
}

#endif //ZKV_ILSMTREESEGMENT_H
