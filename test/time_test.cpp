#include <gtest/gtest.h>

#include <thread>

#include "core/time.hpp"

using namespace frankie::core;

TEST(WallClockMsTest, ReturnsNonZero) {
  auto ts = wall_clock_ms();
  EXPECT_GT(ts, 0u);
}

TEST(WallClockMsTest, ReturnsPlausibleUnixTimestamp) {
  auto ts = wall_clock_ms();
  // After 2024-01-01 00:00:00 UTC
  constexpr std::uint64_t kJan2024Ms = 1'704'067'200'000ULL;
  // Before 2100-01-01 00:00:00 UTC
  constexpr std::uint64_t kJan2100Ms = 4'102'444'800'000ULL;
  EXPECT_GE(ts, kJan2024Ms);
  EXPECT_LE(ts, kJan2100Ms);
}

TEST(WallClockMsTest, NonDecreasingAcrossCalls) {
  auto t1 = wall_clock_ms();
  auto t2 = wall_clock_ms();
  EXPECT_GE(t2, t1);
}

TEST(WallClockMsTest, AdvancesAfterSleep) {
  auto t1 = wall_clock_ms();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  auto t2 = wall_clock_ms();
  EXPECT_GT(t2, t1);
}
