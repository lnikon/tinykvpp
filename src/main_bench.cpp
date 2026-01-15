#include <benchmark/benchmark.h>

int SomeFunction() {
  int a = 4;
  int b = 5;
  return a + b;
}

static void BM_SomeFunction(benchmark::State &state) {
  // Perform setup here
  for (auto _ : state) {
    // This code gets timed
    SomeFunction();
  }
}
// Register the function as a benchmark
BENCHMARK(BM_SomeFunction);
// Run the benchmark
BENCHMARK_MAIN();
