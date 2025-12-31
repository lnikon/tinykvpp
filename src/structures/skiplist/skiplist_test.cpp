#include <gtest/gtest.h>

#include <algorithm>

#include "structures/skiplist/skiplist.h"

using namespace structures;

struct test_record_t
{
    using key_t = std::string;
    using value_t = std::string;

    key_t   m_key;
    value_t m_value;
};

using test_skiplist_t = skiplist::skiplist_t<test_record_t, std::less<>>;

TEST(SkipListTest, EmplaceAndFind)
{
    test_skiplist_t sl;

    sl.emplace({"rec1", "val1"});
    sl.emplace({"rec3", "val3"});
    sl.emplace({"rec2", "val2"});

    // Get what we put
    auto rec = sl.find("rec2");
    EXPECT_TRUE(rec.has_value());
    EXPECT_EQ(rec->m_key, "rec2");
    EXPECT_EQ(rec->m_value, "val2");

    // Return nullopt on non-existing keys
    rec = sl.find("nonexst");
    EXPECT_FALSE(rec.has_value());

    sl.emplace({"rec3", "val4"});
    rec = sl.find("rec3");
    EXPECT_TRUE(rec.has_value());
    EXPECT_EQ(rec->m_key, "rec3");
    EXPECT_EQ(rec->m_value, "val4");
}

TEST(SkipListTest, CalcSizeWithIters)
{
    test_skiplist_t sl;

    sl.emplace({"rec1", "val1"});
    sl.emplace({"rec3", "val3"});
    sl.emplace({"rec2", "val2"});

    auto size{0U};
    for (auto begin{sl.begin()}; begin != sl.end(); ++begin)
    {
        size++;
    }

    EXPECT_EQ(size, sl.size());
}

TEST(SkipListTest, IterValid)
{
    test_skiplist_t sl;

    sl.emplace({"rec1", "val1"});
    sl.emplace({"rec3", "val3"});
    sl.emplace({"rec2", "val2"});

    auto begin{sl.begin()};
    EXPECT_EQ(begin->m_key, "rec1");

    begin++;
    EXPECT_EQ(begin->m_key, "rec2");
}

TEST(SkipListTest, StdFindIf)
{
    test_skiplist_t skiplist;

    skiplist.emplace({.m_key = "rec1", .m_value = "val1"});
    skiplist.emplace({.m_key = "rec3", .m_value = "val3"});
    skiplist.emplace({.m_key = "rec2", .m_value = "val2"});

    {
        auto iter = std::ranges::find_if(
            skiplist, [](const test_record_t &record) -> bool { return record.m_key == "rec1"; }
        );
        EXPECT_NE(iter, skiplist.end());
        EXPECT_EQ(iter->m_key, "rec1");
        EXPECT_EQ(iter->m_value, "val1");
    }

    {
        auto it = std::ranges::find_if(
            skiplist, [](const test_record_t &record) -> bool { return record.m_key == "rec4"; }
        );
        EXPECT_EQ(it, skiplist.end());
    }
}
