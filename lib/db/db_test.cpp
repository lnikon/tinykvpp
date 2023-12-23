#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <config/config.h>
#include <db/db.h>
#include <structures/lsmtree/lsmtree.h>

TEST_CASE("db interface validation", "[db]") {
  config::sptr_t pConfig{config::make_shared()};
	auto pSegmentStorage {db::lsmtree::segment_storage::make_shared()};
  auto pSegmentManager {db::lsmtree::segment_manager::make_shared(pConfig, pSegmentStorage)};
  auto lsmTree {structures::lsmtree::lsmtree_t{pConfig, pSegmentManager}};

  SECTION("fail when db path is empty") {
    db::db_t db(pConfig);
    REQUIRE(db.open() == false);
  }
}
