//
// Created by nikon on 1/22/22.
//

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "skiplist.h"

#include <spdlog/spdlog.h>

using namespace structures;

struct test_record_t
{
    using key_t = std::string;
    using value_t = std::string;

    key_t m_key;
    value_t m_value;
};

using test_skiplist_t = skiplist::skiplist_t<test_record_t, std::less<>>;

TEST_CASE("Emplace and Find", "[SkipList]")
{
    test_skiplist_t sl;

    sl.emplace({"rec1", "val1"});
    sl.emplace({"rec3", "val3"});
    sl.emplace({"rec2", "val2"});

    // Get what we put
    auto rec = sl.find("rec2");
    REQUIRE(rec.has_value());
    REQUIRE(rec->m_key == "rec2");
    REQUIRE(rec->m_value == "val2");

    // Return nullopt on non-existing keys
    rec = sl.find("nonexst");
    REQUIRE_FALSE(rec.has_value());

    sl.emplace({"rec3", "val4"});
    rec = sl.find("rec3");
    REQUIRE(rec.has_value());
    REQUIRE(rec->m_key == "rec3");
    REQUIRE(rec->m_value == "val4");
}

TEST_CASE("Calculate size using iterators", "[SkipList]")
{
    test_skiplist_t sl;

    sl.emplace({"rec1", "val1"});
    sl.emplace({"rec3", "val3"});
    sl.emplace({"rec2", "val2"});

    auto size{0};
    for (auto begin{sl.begin()}; begin != sl.end(); ++begin)
    {
        spdlog::info("key={} info={}", begin->m_key, begin->m_value);
        size++;
    }

    REQUIRE(size == sl.size());
}

TEST_CASE("Check records pointed by iterators", "[SkipList]")
{
    test_skiplist_t sl;

    sl.emplace({"rec1", "val1"});
    sl.emplace({"rec3", "val3"});
    sl.emplace({"rec2", "val2"});

    auto begin{sl.begin()};
    REQUIRE(begin->m_key == "rec1");
    begin++;
    REQUIRE(begin->m_key == "rec2");
    //    begin++;
    //    REQUIRE(begin->m_key == "rec3");
    //    begin++;
    //    REQUIRE(begin == sl.end());
}

TEST_CASE("std::find_if", "[SkipList]")
{
    test_skiplist_t sl;

    sl.emplace({"rec1", "val1"});
    sl.emplace({"rec3", "val3"});
    sl.emplace({"rec2", "val2"});

    auto it =
        std::find_if(std::begin(sl), std::end(sl), [](const test_record_t &record) { return record.m_key == "rec1"; });
    STATIC_CHECK(std::is_same_v<decltype(it), test_skiplist_t::iterator>);
    REQUIRE(it != sl.end());
    REQUIRE(it->m_key == "rec1");
    REQUIRE(it->m_value == "val1");

    it = std::find_if(std::begin(sl), std::end(sl), [](const test_record_t &record) { return record.m_key == "rec4"; });
    STATIC_CHECK(std::is_same_v<decltype(it), test_skiplist_t::iterator>);
    REQUIRE(it == sl.end());
}
