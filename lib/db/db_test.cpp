#include <catch2/catch_test_macros.hpp>

#include <config/config.h>
#include <db/db.h>
#include <structures/lsmtree/lsmtree.h>


TEST_CASE("db interface validation", "[db]")
{
    config::shared_ptr_t pConfig{config::make_shared()};
    auto pSegmentStorage{db::lsmtree::segments::storage::make_shared()};
    auto lsmTree{structures::lsmtree::lsmtree_t{pConfig}};

    SECTION("fail when db path is empty")
    {
        db::db_t db(pConfig);
        REQUIRE(db.open() == false);
    }
}

