#pragma once

namespace structures::lsmtree::segments
{

struct segment_config_t
{
    const bool DefaultPrepopulateSegmentIndex{false};
    bool       PrepopulateSegmentIndex{DefaultPrepopulateSegmentIndex};
};

} // namespace structures::lsmtree::segments
