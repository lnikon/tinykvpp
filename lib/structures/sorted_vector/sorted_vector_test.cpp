//
// Created by nikon on 1/22/22.
//

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "sorted_vector.h"

#include <spdlog/spdlog.h>

using namespace structures;

struct test_record_t
{
    using key_t = std::string;
    using value_t = std::string;

    key_t m_key;
    value_t m_value;
};

struct record_comparator_by_key_t
{
    bool operator()(const test_record_t &lhs, const test_record_t &rhs)
    {
        return lhs.m_key < rhs.m_key;
    }
};

using test_sorted_vector_t = structures::sorted_vector::sorted_vector_t<test_record_t, record_comparator_by_key_t>;

TEST_CASE("Emplace and Find", "[SortedVector]")
{
    test_sorted_vector_t sv;

    sv.emplace({"rec1", "val1"});
    sv.emplace({"rec3", "val3"});
    sv.emplace({"rec2", "val2"});

    // Get what we put
    auto rec = sv.find({"rec2"}, record_comparator_by_key_t{});
    REQUIRE(rec.has_value());
    REQUIRE(rec->m_key == "rec2");
    REQUIRE(rec->m_value == "val2");

    // Return nullopt on non-existing keys
    rec = sv.find({"nonexst"});
    REQUIRE_FALSE(rec.has_value());

    // Allow overrides
    sv.emplace({"rec3", "val4"});
    rec = sv.find({"rec3"});
    REQUIRE(rec.has_value());
    REQUIRE(rec->m_key == "rec3");
    REQUIRE(rec->m_value == "val4");
}

TEST_CASE("Calculate size using iterators", "[SortedVector]")
{
    test_sorted_vector_t sv;

    sv.emplace({"rec1", "val1"});
    sv.emplace({"rec3", "val3"});
    sv.emplace({"rec2", "val2"});

    auto size{0};
    for (auto begin{sv.begin()}; begin != sv.end(); ++begin)
    {
        // spdlog::info("key={} info={}", begin->m_key, begin->m_value);
        size++;
    }

    REQUIRE(size == sv.size());
}

TEST_CASE("Check records pointed by iterators", "[SortedVector]")
{
    test_sorted_vector_t sv;

    sv.emplace({"rec1", "val1"});
    sv.emplace({"rec3", "val3"});
    sv.emplace({"rec2", "val2"});

    auto begin{sv.begin()};
    REQUIRE(begin->m_key == "rec1");
    begin++;
    REQUIRE(begin->m_key == "rec2");
    begin++;
    REQUIRE(begin->m_key == "rec3");
    begin++;
    REQUIRE(begin == sv.end());
}

TEST_CASE("data is sorted", "[SortedVector]")
{
    test_sorted_vector_t sv;

    sv.emplace({"rec1", "val1"});
    sv.emplace({"rec3", "val3"});
    sv.emplace({"rec2", "val2"});

    test_sorted_vector_t::index_type prev = 0, next = 1;
    for (; next < sv.size(); prev++, next++)
    {
        REQUIRE(record_comparator_by_key_t{}(sv.at(prev), sv.at(next)));
    }
}
TEST_CASE("std::find_if", "[SortedVector]")
{
    test_sorted_vector_t sv;

    sv.emplace({"rec1", "val1"});
    sv.emplace({"rec3", "val3"});
    sv.emplace({"rec2", "val2"});

    auto it = std::find_if(std::begin(sv), std::end(sv), [](test_record_t record) { return record.m_key == "rec1"; });
    STATIC_CHECK(std::is_same_v<decltype(it), test_sorted_vector_t::iterator>);
    REQUIRE(it != sv.end());
    REQUIRE(it->m_key == "rec1");
    REQUIRE(it->m_value == "val1");

    it = std::find_if(std::begin(sv), std::end(sv), [](test_record_t record) { return record.m_key == "rec4"; });
    STATIC_CHECK(std::is_same_v<decltype(it), test_sorted_vector_t::iterator>);
    REQUIRE(it == sv.end());
}
