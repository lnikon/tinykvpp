//
// Created by nikon on 1/22/22.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "MemTable.h"

using structure::memtable::MemTable;
using Record = structure::memtable::MemTable::Record;
using Key = structure::memtable::MemTable::Record::Key;
using Value = structure::memtable::MemTable::Record::Value;

TEST_CASE("Emplace and find", "[MemTable]")
{
    MemTable mt;
    mt.emplace(Record{Key{"B"}, Value{123}});
    mt.emplace(Record{Key{"A"}, Value{-12}});
    mt.emplace(Record{Key{"Z"}, Value{34.44}});
    mt.emplace(Record{Key{"C"}, Value{"Hello"}});

    auto record = mt.find(Key{"C"});
    REQUIRE(record->GetKey() == Key{"C"});
    REQUIRE(record->GetValue() == Value{"Hello"});

    record = mt.find(Key{"V"});
    REQUIRE(record == std::nullopt);
}

TEST_CASE("Check size", "[MemTable]")
{
    MemTable mt;

    {
        Key k{"B"};
        Value v{"123"};

        mt.emplace(Record{k, v});

        auto record = mt.find(Key{"B"});
        REQUIRE(record != std::nullopt);

        size_t actualSize = record->Size();
        size_t expectedSize = k.Size() + v.Size();
        REQUIRE(actualSize == expectedSize);
    }

    {
        Key k{"B"};
        Value v{123};

        mt.emplace(Record{k, v});

        auto record = mt.find(Key{"B"});
        REQUIRE(record != std::nullopt);

        size_t actualSize = record->Size();
        size_t expectedSize = k.Size() + v.Size();
        REQUIRE(actualSize == expectedSize);
    }

    {
        Key k{"B"};
        Value v{123.456};

        mt.emplace(Record{k, v});

        auto record = mt.find(Key{"B"});
        REQUIRE(record != std::nullopt);

        size_t actualSize = record->Size();
        size_t expectedSize = k.Size() + v.Size();
        REQUIRE(actualSize == expectedSize);
    }
}

