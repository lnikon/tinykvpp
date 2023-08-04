//
// Created by nikon on 2/6/22.
//

//
// Created by nikon on 1/22/22.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "LSMTree.h"
#include "LSMTreeTypes.h"

const std::string componentName = "[LSMTree]";

using namespace structures;

TEST_CASE("Emplace and Find", componentName) {
  spdlog::set_level(spdlog::level::debug);
  lsmtree::LSMTreeConfig config;
  config.DiskFlushThresholdSize = 24;
  config.SegmentType = lsmtree::LSMTreeSegmentType::Mock;

  SECTION("Put and Get") {
    lsmtree::LSMTree lsmTree(config);
    lsmTree.Put(lsmtree::key_t{"B"}, lsmtree::value_t{4});
    lsmTree.Put(lsmtree::key_t{"A"}, lsmtree::value_t{5});
    lsmTree.Put(lsmtree::key_t{"E"}, lsmtree::value_t{3});
    lsmTree.Put(lsmtree::key_t{"Z"}, lsmtree::value_t{2});
    lsmTree.Put(lsmtree::key_t{"K"}, lsmtree::value_t{1});
  }
}
