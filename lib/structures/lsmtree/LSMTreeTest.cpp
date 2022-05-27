//
// Created by nikon on 2/6/22.
//

//
// Created by nikon on 1/22/22.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "LSMTreeTypes.h"
#include "LSMTree.h"

using namespace structures::lsmtree;
const std::string componentName = "[LSMTree]";

TEST_CASE("Emplace and Find", componentName)
{

    LSMTreeConfig config;
    config.DiskFlushThresholdSize = 24;
    config.SegmentType = LSMTreeSegmentType::Mock;
    LSMTree lsmTree(config);

    lsmTree.Put(Key{"B"}, Value{4});
    lsmTree.Put(Key{"A"}, Value{5});
    lsmTree.Put(Key{"E"}, Value{3});
    lsmTree.Put(Key{"Z"}, Value{2});
    lsmTree.Put(Key{"K"}, Value{1});
}


