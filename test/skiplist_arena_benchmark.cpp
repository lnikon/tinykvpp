#include <benchmark/benchmark.h>

#include <random>

#include "storage/skiplist_arena.h"
#include "test_common.hpp"

namespace bm = benchmark;

using namespace frankie::storage::arena;
using namespace frankie::testing;

static void skiplist_arena_insertion_bm(bm::State& state) {
  constexpr size_t pool_size = 5'000'000;
  std::vector<std::string> key_pool;
  std::vector<std::string> value_pool;

  std::mt19937_64 rng(42);
  for (size_t i = 0; i < pool_size; ++i) {
    key_pool.push_back(random_string_rng(rng, 16));
    value_pool.push_back(random_string_rng(rng, 128));
  }

  skiplist<> sl;
  std::uint64_t idx = 0;
  for (auto _ : state) {
    sl.insert(key_pool[idx % pool_size], value_pool[idx % pool_size]);
    ++idx;
  }
}
BENCHMARK(skiplist_arena_insertion_bm);

static void skiplist_arena_search_bm(bm::State& state) {
  constexpr size_t pool_size = 10000;
  std::vector<std::string> key_pool;
  std::vector<std::string> value_pool;

  std::mt19937_64 rng(42);
  for (size_t i = 0; i < pool_size; ++i) {
    key_pool.push_back(random_string_rng(rng, 16));
    value_pool.push_back(random_string_rng(rng, 128));
  }

  skiplist<> sl;
  for (size_t idx{0}; idx < pool_size; idx++) {
    sl.insert(key_pool[idx], value_pool[idx]);
  }

  std::uint64_t idx = 0;
  for (auto _ : state) {
    bm::DoNotOptimize(sl.get(key_pool[idx % pool_size]));
    ++idx;
  }
}
BENCHMARK(skiplist_arena_search_bm);
