#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <db/db.h>
#include <structures/lsmtree/lsmtree.h>

TEST_CASE("db interface validation", "[db]") {
	const auto dbPath = std::string("");
  auto lsmTreeConfig = structures::lsmtree::lsmtree_config_t{};
  auto pSegmentManager =
      std::make_shared<structures::lsmtree::lsmtree_segment_manager_t>(dbPath);

  structures::lsmtree::lsmtree_t lsmTree(lsmTreeConfig, pSegmentManager);

  SECTION("fail when db path is empty") {
    db::db_t db({.dbPath = dbPath, .lsmTreeConfig{}});
    REQUIRE(db.open() == false);
  }
}
