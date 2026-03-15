#include <benchmark/benchmark.h>

#include <format>
#include <random>

#include "storage/memtable.hpp"

namespace bm = benchmark;

using namespace frankie::storage;

constexpr size_t pool_size = 1 << 22;
constexpr size_t pool_mask = pool_size - 1;

struct rng_pool {
  std::vector<std::string> keys;
  std::vector<std::string> values;

  rng_pool() {
    keys.reserve(pool_size);
    values.reserve(pool_size);
    std::mt19937_64 rng(42);
    for (size_t i = 0; i < pool_size; ++i) {
      keys.push_back(std::format("k{:016x}", i));
      values.push_back(std::format("v{:016x}", i));
    }
    std::ranges::shuffle(keys, rng);
    std::ranges::shuffle(values, rng);
  }
};

static rng_pool& get_rng_pool() {
  static rng_pool pool;
  return pool;
}

static void memtable_put_bm(bm::State& state) {
  const auto& pool = get_rng_pool();
  std::uint64_t target_size = static_cast<std::uint64_t>(state.range(0));

  memtable mt;
  for (std::uint64_t idx = 0; idx < target_size; ++idx) {
    mt.put(pool.keys[idx & pool_mask], pool.values[idx & pool_mask], idx, false);
  }

  std::uint64_t idx = target_size;
  for (auto _ : state) {
    mt.put(pool.keys[idx & pool_mask], pool.values[idx & pool_mask], idx, false);
    ++idx;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(memtable_put_bm)->Range(1024, 1 << 20);
