#include <benchmark/benchmark.h>

#include <random>

#include "storage/skiplist.hpp"
#include "storage/skiplist_pmr.h"
#include "storage/skiplist_arena.h"
#include "storage/skiplist_naive.h"
#include "test_common.hpp"

namespace bm = benchmark;

using namespace frankie::core;
using namespace frankie::storage;
using namespace frankie::testing;

// ---------------------------------------------------------------------------
// skiplist_naive benchmarks (baseline — no arena, std::string nodes)
// ---------------------------------------------------------------------------

static void skiplist_naive_insertion_bm(bm::State& state) {
  constexpr size_t pool_size = 5'000'000;
  std::vector<std::string> key_pool;
  std::vector<std::string> value_pool;

  std::mt19937_64 rng(42);
  for (size_t i = 0; i < pool_size; ++i) {
    key_pool.push_back(random_string_rng(rng, 16));
    value_pool.push_back(random_string_rng(rng, 128));
  }

  frankie::storage::naive::skiplist sl;
  std::uint64_t idx = 0;
  for (auto _ : state) {
    sl.insert(key_pool[idx % pool_size], value_pool[idx % pool_size]);
    ++idx;
  }
}
BENCHMARK(skiplist_naive_insertion_bm);

// static void skiplist_insertion_bm(bm::State& state) {
//   constexpr size_t pool_size = 5'000'000;
//   std::vector<std::string> key_pool;
//   std::vector<std::string> value_pool;
//
//   std::mt19937_64 rng(42);
//   for (size_t i = 0; i < pool_size; ++i) {
//     key_pool.push_back(random_string_rng(rng, 16));
//     value_pool.push_back(random_string_rng(rng, 128));
//   }
//
//   arena arena;
//   skiplist* sl =
//       create_skiplist(&arena, DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
//   std::uint64_t idx = 0;
//   for (auto _ : state) {
//     skiplist_insert(sl, key_pool[idx % pool_size],
//                                       value_pool[idx % pool_size]);
//     idx++;
//   }
// }
// BENCHMARK(skiplist_insertion_bm);

// static void skiplist_search_bm(bm::State& state) {
//   constexpr size_t pool_size = 10000;
//   std::vector<std::string> key_pool;
//   std::vector<std::string> value_pool;
//
//   std::mt19937_64 rng(42);
//   for (size_t i = 0; i < pool_size; ++i) {
//     key_pool.push_back(random_string_rng(rng, 16));
//     value_pool.push_back(random_string_rng(rng, 128));
//   }
//
//   arena arena;
//   skiplist* sl =
//       create_skiplist(&arena, DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
//   for (size_t idx{0}; idx < pool_size; idx++) {
//     skiplist_insert(sl, key_pool[idx % pool_size], value_pool[idx % pool_size]);
//   }
//
//   std::uint64_t idx = 0;
//   for (auto _ : state) {
//     bm::DoNotOptimize(skiplist_search(sl, key_pool[idx % pool_size]));
//   }
// }
// BENCHMARK(skiplist_search_bm);


// ---------------------------------------------------------------------------
// skiplist_arena benchmarks
// ---------------------------------------------------------------------------

static void skiplist_arena_insertion_bm(bm::State& state) {
  constexpr size_t pool_size = 5'000'000;
  std::vector<std::string> key_pool;
  std::vector<std::string> value_pool;

  std::mt19937_64 rng(42);
  for (size_t i = 0; i < pool_size; ++i) {
    key_pool.push_back(random_string_rng(rng, 16));
    value_pool.push_back(random_string_rng(rng, 128));
  }

  custom_arena::skiplist<> sl;
  std::uint64_t idx = 0;
  for (auto _ : state) {
    sl.insert(key_pool[idx % pool_size], value_pool[idx % pool_size]);
    ++idx;
  }
}
BENCHMARK(skiplist_arena_insertion_bm);

static void skiplist_arena_simd_insertion_bm(bm::State& state) {
  constexpr size_t pool_size = 5'000'000;
  std::vector<std::string> key_pool;
  std::vector<std::string> value_pool;

  std::mt19937_64 rng(42);
  for (size_t i = 0; i < pool_size; ++i) {
    key_pool.push_back(random_string_rng(rng, 16));
    value_pool.push_back(random_string_rng(rng, 128));
  }

  custom_arena::skiplist<custom_arena::simd_comparator> sl;
  std::uint64_t idx = 0;
  for (auto _ : state) {
    sl.insert(key_pool[idx % pool_size], value_pool[idx % pool_size]);
    ++idx;
  }
}
BENCHMARK(skiplist_arena_simd_insertion_bm);

// ---------------------------------------------------------------------------
// skiplist_pmr benchmarks
// ---------------------------------------------------------------------------

static void skiplist_pmr_insertion_bm(bm::State& state) {
  constexpr size_t pool_size = 5'000'000;
  std::vector<std::string> key_pool;
  std::vector<std::string> value_pool;

  std::mt19937_64 rng(42);
  for (size_t i = 0; i < pool_size; ++i) {
    key_pool.push_back(random_string_rng(rng, 16));
    value_pool.push_back(random_string_rng(rng, 128));
  }

  frankie::storage::pmr::skiplist<> sl;
  std::uint64_t idx = 0;
  for (auto _ : state) {
    sl.insert(key_pool[idx % pool_size], value_pool[idx % pool_size]);
    ++idx;
  }
}
BENCHMARK(skiplist_pmr_insertion_bm);

// static void skiplist_pmr_search_bm(bm::State& state) {
//   constexpr size_t pool_size = 10000;
//   std::vector<std::string> key_pool;
//   std::vector<std::string> value_pool;
//
//   std::mt19937_64 rng(42);
//   for (size_t i = 0; i < pool_size; ++i) {
//     key_pool.push_back(random_string_rng(rng, 16));
//     value_pool.push_back(random_string_rng(rng, 128));
//   }
//
//   frankie::storage::pmr::skiplist<> sl;
//   for (size_t idx{0}; idx < pool_size; idx++) {
//     sl.insert(key_pool[idx], value_pool[idx]);
//   }
//
//   std::uint64_t idx = 0;
//   for (auto _ : state) {
//     bm::DoNotOptimize(sl.get(key_pool[idx % pool_size]));
//     ++idx;
//   }
// }
// BENCHMARK(skiplist_pmr_search_bm);

