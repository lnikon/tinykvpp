#include <chrono>
#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <thread>

#include "structures/lsmtree/lsmtree.h"
#include "config/config.h"
#include "common/helpers.h"

namespace
{

constexpr auto operator""_B(unsigned long long int x) -> std::size_t
{
    return x;
}

constexpr auto operator""_KiB(unsigned long long int x) -> std::size_t
{
    return 1024ULL * x;
}

constexpr auto operator""_MiB(unsigned long long int x) -> std::size_t
{
    return 1024_KiB * x;
}

constexpr auto operator""_GiB(unsigned long long int x) -> std::size_t
{
    return 1024_MiB * x;
}

auto generateConfig(
    std::uint64_t memtableThreshold,
    std::uint64_t levelZeroThreshold,
    std::int64_t  levelNonZeroThreshold
)
{
    auto pResult{config::make_shared()};
    pResult->LSMTreeConfig.DiskFlushThresholdSize = memtableThreshold;
    pResult->LSMTreeConfig.LevelZeroCompactionThreshold = levelZeroThreshold;
    pResult->LSMTreeConfig.LevelNonZeroCompactionThreshold = levelNonZeroThreshold;
    return pResult;
}

auto generateLSMTree(config::shared_ptr_t pConfig)
{
    auto pManifest{db::manifest::make_shared(
        pConfig->manifest_path(),
        wal::wal_builder_t{}
            .build<db::manifest::manifest_t::record_t>(wal::log_storage_type_k::in_memory_k)
            .value()
    )};
    auto pWAL{
        wal::wal_builder_t{}.build<raft::v1::LogEntry>(wal::log_storage_type_k::in_memory_k).value()
    };

    return structures::lsmtree::lsmtree_builder_t{}.build(pConfig, pManifest, pWAL);
}

using record_t = structures::memtable::memtable_t::record_t;

} // namespace

namespace raft::v1
{
template <typename TStream> auto operator>>(TStream &stream, LogEntry &record) -> TStream &
{
    record.ParseFromString(stream.str());
    return stream;
}
} // namespace raft::v1

TEST(LSMTreeTestBasicCRUD, PutAndGet)
{
    auto pConfig{generateConfig(16_B, 64_B, 128_B)};
    auto pLsm{generateLSMTree(pConfig)};

    record_t rec;
    rec.m_key.m_key = "Alice";
    rec.m_value.m_value = "123";

    auto status{pLsm->put(rec)};
    EXPECT_TRUE(status.has_value());

    auto expected{pLsm->get(rec.m_key)};
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(expected->m_key, rec.m_key);
    EXPECT_EQ(expected->m_value, rec.m_value);
}

TEST(LSMTreeTestBasicCRUD, PutMultipleRecords)
{
    const auto &randomKeys = common::generateRandomStringPairVector(16_B);
    auto        pConfig{generateConfig(16_B, 64_B, 128_B)};
    auto        pLsm{generateLSMTree(pConfig)};

    for (const auto &kv : randomKeys)
    {
        auto rec{record_t{}};
        rec.m_key.m_key = kv.first;
        rec.m_value.m_value = kv.second;
        auto status{pLsm->put(rec)};
        EXPECT_TRUE(status.has_value());
    }

    for (const auto &kv : randomKeys)
    {
        auto rec{record_t{}};
        rec.m_key.m_key = kv.first;
        rec.m_value.m_value = kv.second;

        auto expected = pLsm->get(rec.m_key);

        EXPECT_TRUE(expected.has_value());
        EXPECT_EQ(expected.value().m_key, rec.m_key);
        EXPECT_EQ(expected.value().m_value, rec.m_value);
    }
}

TEST(LSMTreeTestBasicCRUD, GetNonExistentKey)
{
    const auto &randomKeys = common::generateRandomStringPairVector(16_B);
    auto        pConfig{generateConfig(16_B, 64_B, 128_B)};
    auto        pLsm{generateLSMTree(pConfig)};

    record_t rec;
    rec.m_key.m_key = "Alice";
    rec.m_value.m_value = "123";

    auto expected{pLsm->get(rec.m_key)};
    EXPECT_FALSE(expected.has_value());
}

TEST(LSMTreeTestBasicCRUD, UpdateExistingKey)
{
    const auto &randomKeys = common::generateRandomStringPairVector(16_B);
    auto        pConfig{generateConfig(16_B, 64_B, 128_B)};
    auto        pLsm{generateLSMTree(pConfig)};

    record_t rec;
    rec.m_key.m_key = "Alice";
    rec.m_value.m_value = "123";

    auto status{pLsm->put(rec)};
    EXPECT_TRUE(status.has_value());

    auto expected{pLsm->get(rec.m_key)};
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(expected->m_key, rec.m_key);
    EXPECT_EQ(expected->m_value, rec.m_value);

    rec.m_value.m_value = "456";
    status = pLsm->put(rec);
    EXPECT_TRUE(status.has_value());

    expected = pLsm->get(rec.m_key);
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(expected->m_key, rec.m_key);
    EXPECT_EQ(expected->m_value, rec.m_value);
}

TEST(LSMTreeTestBasicCRUD, EmptyTreeGet)
{
    const auto &randomKeys = common::generateRandomStringPairVector(16_B);
    auto        pConfig{generateConfig(16_B, 64_B, 128_B)};
    auto        pLsm{generateLSMTree(pConfig)};

    record_t rec;
    rec.m_key.m_key = "Alice";
    rec.m_value.m_value = "123";

    auto expected{pLsm->get(rec.m_key)};
    EXPECT_FALSE(expected.has_value());
}

TEST(LSMTreeTestBasicCRUD, LargeKeyValuePair)
{
    auto pConfig{generateConfig(16_MiB, 64_MiB, 128_MiB)};
    auto pLsm{generateLSMTree(pConfig)};

    record_t rec;
    rec.m_key.m_key = common::generateRandomString(4_MiB);
    rec.m_value.m_value = common::generateRandomString(8_MiB);

    auto status{pLsm->put(rec)};
    EXPECT_TRUE(status.has_value());

    auto expected{pLsm->get(rec.m_key)};
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(expected->m_key, rec.m_key);
    EXPECT_EQ(expected->m_value, rec.m_value);
}

TEST(LSMTreeTestBasicCRUD, LargeKeyValuePairFlush)
{
    auto pConfig{generateConfig(16_MiB, 64_MiB, 128_MiB)};
    auto pLsm{generateLSMTree(pConfig)};

    record_t rec;
    rec.m_key.m_key = common::generateRandomString(14_MiB);
    rec.m_value.m_value = common::generateRandomString(18_MiB);

    auto status{pLsm->put(rec)};
    EXPECT_TRUE(status.has_value());

    auto expected{pLsm->get(rec.m_key)};
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(expected->m_key, rec.m_key);
    EXPECT_EQ(expected->m_value, rec.m_value);
}

TEST(LSMTreeTestBasicCRUD, MultipleLargeKeyValuePairsFlush)
{
    auto pConfig{generateConfig(2_MiB, 4_MiB, 8_MiB)};
    auto pLsm{generateLSMTree(pConfig)};

    const auto            recordCount{16};
    std::vector<record_t> records;
    records.reserve(recordCount);

    auto count{recordCount};
    while ((count--) != 0ULL)
    {
        record_t rec;
        rec.m_key.m_key = common::generateRandomString(64_KiB);
        rec.m_value.m_value = common::generateRandomString(64_KiB);
        records.emplace_back(std::move(rec));
    }

    for (const auto &rec : records)
    {
        auto status{pLsm->put(rec)};
        EXPECT_TRUE(status.has_value());
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::size_t idx{0};
    for (const auto &rec : records)
    {
        spdlog::info("Verifying record {}/{}", ++idx, recordCount);
        auto expected{pLsm->get(rec.m_key)};
        EXPECT_TRUE(expected.has_value());
        spdlog::info(
            "expected->m_key.m_key.size()={}, rec.m_key.m_key={}",
            expected->m_key.m_key.size(),
            rec.m_key.m_key.size()
        );
        EXPECT_EQ(expected->m_key.m_key, rec.m_key.m_key);
        EXPECT_EQ(expected->m_value.m_value, rec.m_value.m_value);
        ++idx;
    }
}

TEST(LSMTreeTestBasicCRUD, MultipleSmallKeyValuePairsFlush)
{
    auto pConfig{generateConfig(16_KiB, 64_KiB, 128_KiB)};
    auto pLsm{generateLSMTree(pConfig)};

    const auto            recordCount{16};
    std::vector<record_t> records;
    records.reserve(recordCount);

    auto count{recordCount};
    while ((count--) != 0ULL)
    {
        record_t rec;
        rec.m_key.m_key = common::generateRandomString(14_KiB);
        rec.m_value.m_value = common::generateRandomString(18_KiB);
        records.emplace_back(std::move(rec));
    }

    for (const auto &rec : records)
    {
        auto status{pLsm->put(rec)};
        EXPECT_TRUE(status.has_value());
    }

    for (const auto &rec : records)
    {
        auto expected{pLsm->get(rec.m_key)};
        EXPECT_TRUE(expected.has_value());
        EXPECT_EQ(expected->m_key, rec.m_key);
        EXPECT_EQ(expected->m_value, rec.m_value);
    }
}

TEST(LSMTreeTestBasicCRUD, EmptyKeyValue)
{
    auto pConfig{generateConfig(16_B, 64_B, 128_B)};
    auto pLsm{generateLSMTree(pConfig)};

    // Empty key and value
    record_t rec;
    rec.m_key.m_key = "";
    rec.m_value.m_value = "";

    auto status{pLsm->put(rec)};
    EXPECT_TRUE(status.has_value());

    auto expected{pLsm->get(rec.m_key)};
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(expected->m_key, rec.m_key);
    EXPECT_EQ(expected->m_value, rec.m_value);

    // Empty value
    rec.m_key.m_key = "alice";
    rec.m_value.m_value = "";

    status = pLsm->put(rec);
    EXPECT_TRUE(status.has_value());

    expected = pLsm->get(rec.m_key);
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(expected->m_key, rec.m_key);
    EXPECT_EQ(expected->m_value, rec.m_value);

    // Empty key
    rec.m_key.m_key = "";
    rec.m_value.m_value = "123";

    status = pLsm->put(rec);
    EXPECT_TRUE(status.has_value());

    expected = pLsm->get(rec.m_key);
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(expected->m_key, rec.m_key);
    EXPECT_EQ(expected->m_value, rec.m_value);
}

int main(int argc, char **argv)
{
    spdlog::set_level(spdlog::level::debug);

    // std::filesystem::create_directory("./segments");

    testing::InitGoogleTest(&argc, argv);
    auto res{RUN_ALL_TESTS()};

    // std::filesystem::remove_all("./segments");

    return res;
}
