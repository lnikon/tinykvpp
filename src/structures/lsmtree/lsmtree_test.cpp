#include <limits>
#include <random>
#include <filesystem>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <thread>

#include "structures/lsmtree/lsmtree.h"
#include "config/config.h"

namespace
{
template <typename TNumber>
auto generateRandomNumber(
    const TNumber min = std::numeric_limits<TNumber>::min(),
    const TNumber max = std::numeric_limits<TNumber>::max()
) noexcept -> TNumber
{
    std::mt19937 rng{std::random_device{}()};
    if constexpr (std::is_same_v<int, TNumber>)
    {
        return std::uniform_int_distribution<TNumber>(min, max)(rng);
    }
    else if (std::is_same_v<std::size_t, TNumber>)
    {
        return std::uniform_int_distribution<TNumber>(min, max)(rng);
    }
    else if (std::is_same_v<double, TNumber>)
    {
        return std::uniform_real_distribution<double>(min, max)(rng);
    }
    else if (std::is_same_v<float, TNumber>)
    {
        return std::uniform_real_distribution<float>(min, max)(rng);
    }
    else
    {
        // TODO(vahag): better handle this case
        return 0;
    }
}

auto generateRandomString(std::size_t length) noexcept -> std::string
{
    static const auto &alphabet = "0123456789"
                                  "abcdefghijklmnopqrstuvwxyz"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    std::string result;
    result.reserve(length);
    while ((length--) != 0U)
    {
        result += alphabet[generateRandomNumber<std::size_t>(0, sizeof(alphabet) - 2)];
    }

    return result;
}

auto generateRandomStringPairVector(const std::size_t length) noexcept
    -> std::vector<std::pair<std::string, std::string>>
{
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(length);
    for (std::string::size_type size = 0; size < length; size++)
    {
        result.emplace_back(
            generateRandomString(generateRandomNumber<std::size_t>(64, 64)),
            generateRandomString(generateRandomNumber<std::size_t>(64, 64))
        );
    }
    return result;
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

TEST(LSMTreeTest, FlushRegularSegment)
{
    using namespace structures;

    spdlog::set_level(spdlog::level::debug);

    const auto &randomKeys = generateRandomStringPairVector(16);

    auto pConfig{generateConfig(16, 64, 128)};

    auto pManifest{db::manifest::make_shared(
        pConfig->manifest_path(),
        wal::wal_builder_t{}
            .build<db::manifest::manifest_t::record_t>(wal::log_storage_type_k::in_memory_k)
            .value()
    )};
    auto pWAL{
        wal::wal_builder_t{}.build<raft::v1::LogEntry>(wal::log_storage_type_k::in_memory_k).value()
    };
    auto pLsm{structures::lsmtree::lsmtree_builder_t{}.build(pConfig, pManifest, pWAL)};

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
