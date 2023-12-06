#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <config/config.h>
#include <db/db.h>
#include <structures/lsmtree/lsmtree.h>

TEST_CASE("db interface validation", "[db]") {
  config::sptr_t pConfig{config::make_shared()};
  auto pSegmentManager =
      std::make_shared<structures::lsmtree::lsmtree_segment_manager_t>(pConfig);

  structures::lsmtree::lsmtree_t lsmTree(pConfig, pSegmentManager);

  SECTION("fail when db path is empty") {
    db::db_t db(pConfig);
    REQUIRE(db.open() == false);
  }
}
