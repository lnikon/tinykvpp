#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <spdlog/spdlog.h>
#include <structures/lsmtree/lsmtree.h>

#include <limits>
#include <random>
#include <iostream>

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

class RemoveArtefactsListener : public Catch::EventListenerBase
{
  public:
    using Catch::EventListenerBase::EventListenerBase;

    void prepare()
    {
        std::filesystem::remove_all("segments");
        std::filesystem::remove("segments");
        std::filesystem::create_directory("segments");
        std::filesystem::remove("manifest");
        std::filesystem::remove("wal");
    }

    /// cppcheck-suppress unusedFunction
    void testCaseStarting(Catch::TestCaseInfo const &testInfo) override
    {
        (void)testInfo;
        prepare();
    }

    /// cppcheck-suppress unusedFunction
    void testCaseEnded(Catch::TestCaseStats const &testCaseStats) override
    {
        (void)testCaseStats;
        prepare();
    }
};

CATCH_REGISTER_LISTENER(RemoveArtefactsListener)

namespace
{
template <typename TNumber>
auto generateRandomNumber(const TNumber min = std::numeric_limits<TNumber>::min(),
                          const TNumber max = std::numeric_limits<TNumber>::max()) noexcept
    -> TNumber
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
        result.emplace_back(generateRandomString(generateRandomNumber<std::size_t>(64, 64)),
                            generateRandomString(generateRandomNumber<std::size_t>(64, 64)));
    }
    return result;
}

inline constexpr std::string_view componentName = "[LSMTree]";
} // namespace

TEST_CASE("Flush regular segment", std::string(componentName))
{
    using namespace structures;

    // Disable logging to save on text execution time
    spdlog::set_level(spdlog::level::info);

    auto randomKeys = generateRandomStringPairVector(16);

    SECTION("Put and Get")
    {
        auto pConfig{config::make_shared()};
        pConfig->LSMTreeConfig.DiskFlushThresholdSize = 1; // 64mb = 64000000

        auto               manifest{db::manifest::make_shared(pConfig)};
        auto               wal{wal::make_shared("wal")};
        auto               lsmTree{structures::lsmtree::lsmtree_t{pConfig, manifest, wal}};
        lsmtree::lsmtree_t lsmt(pConfig, manifest, wal);
        for (const auto &kv : randomKeys)
        {
            lsmt.put(lsmtree::key_t{kv.first}, lsmtree::value_t{kv.second});
        }

        for (const auto &kv : randomKeys)
        {
            REQUIRE(lsmt.get(lsmtree::key_t{kv.first}).value().m_key == lsmtree::key_t{kv.first});
            REQUIRE(lsmt.get(lsmtree::key_t{kv.first}).value().m_value ==
                    lsmtree::value_t{kv.second});
        }
    }

    // SECTION("Flush segment when memtable is full")
    // {
    //     config::shared_ptr_t pConfig{config::make_shared()};
    //     pConfig->LSMTreeConfig.DiskFlushThresholdSize = 128;
    //
    //     auto manifest{db::manifest::make_shared(pConfig)};
    //     auto wal{wal::make_shared("wal")};
    //     lsmtree::lsmtree_t lsmt(pConfig, manifest, wal);
    //     for (const auto &kv : randomKeys)
    //     {
    //         lsmt.put(lsmtree::key_t{kv.first}, lsmtree::value_t{kv.second});
    //     }
    //
    //     REQUIRE(std::filesystem::exists("segments"));
    //     REQUIRE(!std::filesystem::is_empty("segments"));
    // }
}
