#include "test_common.hpp"

#include <random>

namespace frankie::testing {

// std::string random_string(std::uint64_t length) noexcept {
//   std::mt19937_64 gen(std::random_device{}());
//
//   std::string result;
//   result.reserve(length);
//
//   std::uint64_t remaining = length;
//   while (remaining >= sizeof(std::uint64_t)) {
//     std::uint64_t chunk = gen();  // mt19937_64 already produces 64-bit
//     values result.append(reinterpret_cast<char*>(&chunk),
//     sizeof(std::uint64_t)); remaining -= sizeof(std::uint64_t);
//   }
//
//   if (remaining > 0) {
//     std::uint64_t chunk = gen();
//     result.append(reinterpret_cast<char*>(&chunk), remaining);
//   }
//
//   return result;
// }

std::string random_string(std::uint64_t length) noexcept {
  static constexpr char charset[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  static constexpr auto charset_size = sizeof(charset) - 1;

  std::mt19937_64 gen(std::random_device{}());
  std::uniform_int_distribution<std::size_t> dist(0, charset_size - 1);

  std::string result;
  result.reserve(length);

  for (std::uint64_t i = 0; i < length; ++i) {
    result += charset[dist(gen)];
  }

  return result;
}

std::uint64_t random_u64(std::uint64_t min, std::uint64_t max) noexcept {
  std::mt19937_64 gen(std::random_device{}());
  std::uniform_int_distribution<std::uint64_t> dist(min, max);
  return dist(gen);
}

}  // namespace frankie::testing
