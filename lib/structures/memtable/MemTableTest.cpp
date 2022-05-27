//
// Created by nikon on 1/22/22.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "MemTable.h"

using structures::memtable::MemTable;
using Record = structures::memtable::MemTable::Record;
using Key = structures::memtable::MemTable::Record::Key;
using Value = structures::memtable::MemTable::Record::Value;

TEST_CASE("Emplace and Find", "[MemTable]")
{
    MemTable mt;
    mt.Emplace(Record{Key{"B"}, Value{123}});
    mt.Emplace(Record{Key{"A"}, Value{-12}});
    mt.Emplace(Record{Key{"Z"}, Value{34.44}});
    mt.Emplace(Record{Key{"C"}, Value{"Hello"}});

    auto record = mt.Find(Key{"C"});
    REQUIRE(record->GetKey() == Key{"C"});
    REQUIRE(record->GetValue() == Value{"Hello"});

    record = mt.Find(Key{"V"});
    REQUIRE(record == std::nullopt);
}

TEST_CASE("Check record size before and after insertion", "[MemTable]")
{
    MemTable mt;

    {
        Key k{"B"};
        Value v{"123"};

        mt.Emplace(Record{k, v});

        auto record = mt.Find(Key{"B"});
        REQUIRE(record != std::nullopt);

        size_t actualSize = record->Size();
        size_t expectedSize = k.Size() + v.Size().value();
        REQUIRE(actualSize == expectedSize);
    }

    {
        Key k{"B"};
        Value v{123};

        mt.Emplace(Record{k, v});

        auto record = mt.Find(Key{"B"});
        REQUIRE(record != std::nullopt);

        size_t actualSize = record->Size();
        size_t expectedSize = k.Size() + v.Size().value();
        REQUIRE(actualSize == expectedSize);
    }

    {
        Key k{"B"};
        Value v{123.456};

        auto record = Record{k, v};
        mt.Emplace(record);

        auto recordOpt = mt.Find(k);
        REQUIRE(recordOpt != std::nullopt);
        record = *recordOpt;

        size_t actualSize = record.Size();
        size_t expectedSize = k.Size() + v.Size().value();
        REQUIRE(actualSize == expectedSize);
    }
}

TEST_CASE("Check size", "[MemTable]")
{
    MemTable mt;
    auto k1 = Key{"B"}, k2 = Key{"A"}, k3 = Key{"Z"};
    auto v1 = Value{123}, v2 = Value{34.44}, v3 = Value{"Hello"};
    mt.Emplace(Record{k1, v1});
    mt.Emplace(Record{k2, v2});
    mt.Emplace(Record{k3, v3});

    // TODO: Not sure if this a good/correct way to check MemTable::Size() :)
    REQUIRE(mt.Size() == k1.Size() + v1.Size().value() + k2.Size() + v2.Size().value() + k3.Size() + v3.Size().value());
}

TEST_CASE("Check count", "[MemTable]")
{
    MemTable mt;
    mt.Emplace(Record{Key{"B"}, Value{123}});
    mt.Emplace(Record{Key{"A"}, Value{-12}});
    mt.Emplace(Record{Key{"Z"}, Value{34.44}});
    mt.Emplace(Record{Key{"C"}, Value{"Hello"}});

    REQUIRE(mt.Count() == 4);
}

