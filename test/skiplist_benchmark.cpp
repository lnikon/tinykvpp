#include <benchmark/benchmark.h>

#include <random>

#include "storage/skiplist.hpp"
#include "test_common.hpp"

namespace bm = benchmark;

using namespace frankie::core;
using namespace frankie::storage;
using namespace frankie::testing;

static void skiplist_insertion(bm::State& state) {
  constexpr size_t pool_size = 10000;
  std::vector<std::string> key_pool;
  std::vector<std::string> value_pool;

  std::mt19937_64 rng(42);
  for (size_t i = 0; i < pool_size; ++i) {
    key_pool.push_back(random_string_rng(rng, 16));
    value_pool.push_back(random_string_rng(rng, 128));
  }

  arena arena;
  skiplist* sl =
      create_skiplist(&arena, DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
  std::uint64_t idx = 0;
  for (auto _ : state) {
    skiplist_insert(sl, key_pool[idx % pool_size], value_pool[idx % pool_size]);
  }
}

BENCHMARK(skiplist_insertion);
