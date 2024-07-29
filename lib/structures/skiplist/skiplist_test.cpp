//
// Created by nikon on 1/22/22.
//

#include <catch2/catch_test_macros.hpp>

#include "skiplist.h"

using namespace structures;

TEST_CASE("Emplace and Find", "[MemTable]")
{
    skiplist::skiplist_t sl;

    sl.insert({"rec1", "val1"});
    sl.insert({"rec3", "val3"});
    sl.insert({"rec2", "val2"});

    // Get what we put
    auto rec = sl.find("rec2");
    REQUIRE(rec.has_value());
    REQUIRE(rec->key == "rec2");
    REQUIRE(rec->value == "val2");

    // Return nullopt on non-existing keys
    rec = sl.find("nonexst");
    REQUIRE_FALSE(rec.has_value());

    // Don't override values
    sl.insert({"rec3", "val4"});
    rec = sl.find("rec3");
    REQUIRE(rec.has_value());
    REQUIRE(rec->key == "rec3");
    REQUIRE(rec->value == "val3");
}
