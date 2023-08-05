//
// Created by nikon on 2/6/22.
//

//
// Created by nikon on 1/22/22.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "lsmtree.h"
#include "lsmtree_config.h"
#include "lsmtree_types.h"

inline constexpr std::string componentName = "[LSMTree]";

using namespace structures;

TEST_CASE("Emplace and Find", componentName) {
  lsmtree::lsmtree_config_t config{.DiskFlushThresholdSize = 24,
                                   .SegmentType =
                                       lsmtree::lsmtree_segment_type_t::mock_k};

  SECTION("Put and Get") {
    lsmtree::lsmtree_t lsmt(config);
    lsmt.put(lsmtree::key_t{"B"}, lsmtree::value_t{4});
    lsmt.put(lsmtree::key_t{"A"}, lsmtree::value_t{5});
    lsmt.put(lsmtree::key_t{"E"}, lsmtree::value_t{3});
    lsmt.put(lsmtree::key_t{"Z"}, lsmtree::value_t{2});
    lsmt.put(lsmtree::key_t{"K"}, lsmtree::value_t{1});

    REQUIRE(lsmt.get(lsmtree::key_t{"K"}).value().m_key == lsmtree::key_t{"K"});
    REQUIRE(lsmt.get(lsmtree::key_t{"K"}).value().m_value ==
            lsmtree::value_t{1});
  }
}
