//
// Created by nikon on 1/22/22.
//

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "memtable.h"

using namespace structures;

using structures::memtable::memtable_t;
using record_t = memtable::memtable_t::record_t;
using record_key_t = structures::memtable::memtable_t::record_t::key_t;
using record_value_t = structures::memtable::memtable_t::record_t::value_t;

TEST_CASE("Emplace and Find", "[MemTable]")
{
    memtable_t mt;
    mt.emplace(record_t{record_key_t{"B"}, record_value_t{123}});
    mt.emplace(record_t{record_key_t{"A"}, record_value_t{-12}});
    mt.emplace(record_t{record_key_t{"Z"}, record_value_t{34.44}});
    mt.emplace(record_t{record_key_t{"C"}, record_value_t{"Hello"}});

    auto record = mt.find(record_key_t{"C"});
    REQUIRE(record->m_key == record_key_t{"C"});
    REQUIRE(record->m_value == record_value_t{"Hello"});

    record = mt.find(record_key_t{"V"});
    REQUIRE(record == std::nullopt);
}

TEST_CASE("Check record size before and after insertion", "[MemTable]")
{
    {
        memtable_t mt;
        record_key_t k{"B"};
        record_value_t v{"123"};

        mt.emplace(record_t{k, v});

        auto record = mt.find(record_key_t{"B"});
        REQUIRE(record != std::nullopt);

        size_t actualSize = record->size();
        size_t expectedSize = k.size() + v.size();
        auto kz = k.size();
        auto vz = v.size();
        REQUIRE(actualSize == expectedSize);
    }

    {
        memtable_t mt;
        record_key_t k{"B"};
        record_value_t v{123};

        mt.emplace(record_t{k, v});

        auto record = mt.find(record_key_t{"B"});
        REQUIRE(record != std::nullopt);

        size_t actualSize = record->size();
        auto kz = k.size();
        auto vz = v.size();
        size_t expectedSize = k.size() + v.size();
        REQUIRE(actualSize == expectedSize);
    }

    {
        memtable_t mt;
        record_key_t k{"B"};
        record_value_t v{123.456};

        auto record = record_t{k, v};
        mt.emplace(record);

        auto recordOpt = mt.find(k);
        REQUIRE(recordOpt != std::nullopt);
        record = *recordOpt;

        size_t actualSize = record.size();
        size_t expectedSize = k.size() + v.size();
        REQUIRE(actualSize == expectedSize);
    }
}

TEST_CASE("Check size", "[MemTable]")
{
    memtable_t mt;
    auto k1 = record_key_t{"B"}, k2 = record_key_t{"A"}, k3 = record_key_t{"Z"};
    auto v1 = record_value_t{123}, v2 = record_value_t{34.44}, v3 = record_value_t{"Hello"};
    mt.emplace(record_t{k1, v1});
    mt.emplace(record_t{k2, v2});
    mt.emplace(record_t{k3, v3});

    // TODO: Not sure if this a good/correct way to check MemTable::Size() :)
    REQUIRE(mt.size() == k1.size() + v1.size() + k2.size() + v2.size() + k3.size() + v3.size());
}

TEST_CASE("Check count", "[MemTable]")
{
    memtable_t mt;
    mt.emplace(record_t{record_key_t{"B"}, record_value_t{123}});
    mt.emplace(record_t{record_key_t{"A"}, record_value_t{-12}});
    mt.emplace(record_t{record_key_t{"Z"}, record_value_t{34.44}});
    mt.emplace(record_t{record_key_t{"C"}, record_value_t{"Hello"}});

    REQUIRE(mt.count() == 4);
}
