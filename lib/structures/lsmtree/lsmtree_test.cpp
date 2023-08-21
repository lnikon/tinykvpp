#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "lsmtree.h"
#include "lsmtree_config.h"
#include "lsmtree_types.h"

#include <filesystem>
#include <limits>
#include <random>
#include <ranges>

namespace {
template <typename TNumber>
TNumber generateRandomNumber(
    const TNumber min = std::numeric_limits<TNumber>::min(),
    const TNumber max = std::numeric_limits<TNumber>::max()) noexcept {
  std::mt19937 rg{std::random_device{}()};
  if constexpr (std::is_same_v<int, TNumber>) {
    return std::uniform_int_distribution<TNumber>(min, max)(rg);
  } else if (std::is_same_v<std::size_t, TNumber>) {
    return std::uniform_int_distribution<TNumber>(min, max)(rg);
  } else if (std::is_same_v<double, TNumber>) {
    return std::uniform_real_distribution<double>(min, max)(rg);
  } else if (std::is_same_v<float, TNumber>) {
    return std::uniform_real_distribution<float>(min, max)(rg);
  } else {
    // TODO(vahag): better handle this case
    return 0;
  }
}

std::string generateRandomString(std::size_t length) noexcept {
  static auto &alphabet = "0123456789"
                          "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  std::string result;
  result.reserve(length);
  while (length--) {
    result +=
        alphabet[generateRandomNumber<std::size_t>(0, sizeof(alphabet) - 2)];
  }

  return result;
}

std::vector<std::string>
generateRandomStringVector(const std::size_t length) noexcept {
  std::vector<std::string> result;
  result.reserve(length);
  for (std::string::size_type size = 0; size < length; size++) {
    result.emplace_back(
        generateRandomString(generateRandomNumber<std::size_t>(4, 64)));
  }
  return result;
}

std::vector<std::pair<std::string, std::string>>
generateRandomStringPairVector(const std::size_t length) noexcept {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(length);
  for (std::string::size_type size = 0; size < length; size++) {
    result.emplace_back(
        generateRandomString(generateRandomNumber<std::size_t>(4, 64)),
        generateRandomString(generateRandomNumber<std::size_t>(4, 64)));
  }
  return result;
}

inline constexpr std::string_view componentName = "[LSMTree]";
} // namespace

TEST_CASE("Flush regular segment", std::string(componentName)) {
  using namespace structures;

  lsmtree::lsmtree_config_t config;
  auto randomKeys = generateRandomStringPairVector(1024); SECTION("Put and Get") {
    config.SegmentType = lsmtree::lsmtree_segment_type_t::mock_k;

    auto pSegmentManager =
        std::make_shared<structures::lsmtree::lsmtree_segment_manager_t>();

    lsmtree::lsmtree_t lsmt(config, pSegmentManager);
    for (const auto &kv : randomKeys) {
      lsmt.put(lsmtree::key_t{kv.first}, lsmtree::value_t{kv.second});
    }

    for (const auto &kv : randomKeys) {
      REQUIRE(lsmt.get(lsmtree::key_t{kv.first}).value().m_key ==
              lsmtree::key_t{kv.first});
      REQUIRE(lsmt.get(lsmtree::key_t{kv.first}).value().m_value ==
              lsmtree::value_t{kv.second});
    }
  }

  SECTION("Flush segment when memtable is full") {
    config.DiskFlushThresholdSize = 2048;
    config.SegmentType = lsmtree::lsmtree_segment_type_t::regular_k;

    auto pSegmentManager =
        std::make_shared<structures::lsmtree::lsmtree_segment_manager_t>();

    lsmtree::lsmtree_t lsmt(config, pSegmentManager);
    for (const auto &kv : randomKeys) {
      lsmt.put(lsmtree::key_t{kv.first}, lsmtree::value_t{kv.second});
    }

    auto segmentNames = pSegmentManager->get_segment_names();
    for (const auto &name : segmentNames) {
      // Check that segment was created and dumped into disk
      REQUIRE(std::filesystem::exists(name));

      // Perform a cleanup
      std::filesystem::remove(name);
    }
  }
}
