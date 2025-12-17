#include <benchmark/benchmark.h>
#include <vector>
#include <string>
#include <iostream>
#include <set>

#include "deli.h"
#include "veb.h"
#include "rht.h"

constexpr size_t NUM_KEYS = 1 << 22;
constexpr size_t NUM_QUERIES = 1 << 22;
constexpr size_t low_bits = 20;

template <typename Index>
void bench_insert(Index &index, const char * index_name, std::vector<int> & keys, std::vector<int> & queries) {
  auto bench_func = [&](::benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
      index.insert(keys[i]);
      i = (i == NUM_KEYS - 1) ? 0 : i + 1;
    }
    state.counters["used_keys"] = i;
    state.counters["available_keys"] = NUM_KEYS;
  };
  benchmark::RegisterBenchmark(index_name, bench_func);
}

int main(int argc, char** argv) {
  // Generate a vector of keys
  std::vector<int> keys(NUM_KEYS), queries(NUM_QUERIES);
  for (size_t i = 0; i < NUM_KEYS; ++i) {
    keys[i] = i * 2654435761 % NUM_KEYS;
  }
  for (size_t i = 0; i < NUM_QUERIES; ++i) {
    queries[i] = i * 40503 % NUM_QUERIES;
  }

  DeLI::DeLI<int, low_bits> deli_index;
  bench_insert(deli_index, "DeLI Insert", keys, queries);

  veb_tree veb_index;
  bench_insert(veb_index, "VEB Insert", keys, queries);

  DeLI::RHT<int> rht_index(0, std::numeric_limits<int>::max());
  bench_insert(rht_index, "RHT Insert", keys, queries);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
