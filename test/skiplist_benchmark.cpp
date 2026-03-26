#include <benchmark/benchmark.h>

#include <format>
#include <random>

#include "core/scratch_arena.hpp"
#include "storage/skiplist.hpp"
#include "test_common.hpp"

namespace bm = benchmark;

using namespace frankie::core;
using namespace frankie::storage;
using namespace frankie::testing;

constexpr size_t pool_size = 1 << 22;
constexpr size_t pool_mask = pool_size - 1;

struct rng_pool {
  std::vector<std::string> values;

  rng_pool() {
    values.reserve(pool_size);
    std::mt19937_64 rng(42);
    for (size_t i = 0; i < pool_size; ++i) {
      values.push_back(std::format("k{:016x}", i));
    }
    std::ranges::shuffle(values, rng);
  }
};

static rng_pool &get_rng_pool() {
  static rng_pool pool;
  return pool;
}

static void skiplist_insertion_bm(bm::State &state) {
  const auto &pool = get_rng_pool();
  std::uint64_t target_size = static_cast<std::uint64_t>(state.range(0));

  skiplist<simd_comparator> sl;
  for (std::uint64_t idx = 0; idx < target_size; ++idx) {
    sl.insert(pool.values[idx & pool_mask], pool.values[idx & pool_mask]);
  }

  std::uint64_t idx = target_size;
  for (auto _ : state) {
    sl.insert(pool.values[idx & pool_mask], pool.values[idx & pool_mask]);
    ++idx;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(skiplist_insertion_bm)->Range(1024, 1 << 20);

static void skiplist_scratch_insertion_bm(bm::State &state) {
  const auto &pool = get_rng_pool();
  std::uint64_t target_size = static_cast<std::uint64_t>(state.range(0));

  skiplist<simd_comparator> sl;
  scratch_arena scratch;
  for (std::uint64_t idx = 0; idx < target_size; ++idx) {
    scratch.reset();
    const auto &key = pool.values[idx & pool_mask];
    const auto &value = pool.values[idx & pool_mask];
    std::uint64_t sequence = idx;

    const std::uint64_t total = key.size() + 8 + 8 + 1;
    char *buf = scratch.allocate(total);
    char *p = buf;
    std::memcpy(p, key.data(), key.size());
    p += key.size();
    std::memcpy(p, &sequence, 8);
    p += 8;
    std::memcpy(p, &sequence, 8);
    p += 8;
    *p++ = 0;

    sl.insert(std::string_view{buf, total}, value);
  }

  std::uint64_t idx = target_size;
  for (auto _ : state) {
    scratch.reset();
    const auto &key = pool.values[idx & pool_mask];
    const auto &value = pool.values[idx & pool_mask];
    std::uint64_t sequence = idx;

    const std::uint64_t total = key.size() + 8 + 8 + 1;
    char *buf = scratch.allocate(total);
    char *p = buf;
    std::memcpy(p, key.data(), key.size());
    p += key.size();
    std::memcpy(p, &sequence, 8);
    p += 8;
    std::memcpy(p, &sequence, 8);
    p += 8;
    *p++ = 0;

    sl.insert(std::string_view{buf, total}, value);
    ++idx;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(skiplist_scratch_insertion_bm)->Range(1024, 1 << 20);
