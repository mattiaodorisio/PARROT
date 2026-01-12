#include <benchmark/benchmark.h>
#include <vector>
#include <string>
#include <iostream>
#include <set>
#include <random>

#include "deli.h"
#include "veb.h"
#include "rht.h"

constexpr size_t NUM_KEYS = 1 << 28;
constexpr size_t NUM_QUERIES = 1 << 20;
constexpr size_t low_bits = 20;

template <typename Index>
void bench_insert(Index &index, const char * index_name, std::vector<int> & keys, std::vector<int> & queries) {
  auto bench_func = [&](::benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
      index.insert(keys[i]);
      i = (i == NUM_KEYS - 1) ? 0 : i + 1;
    }
    state.counters["used_keys"] = index.size();
    state.counters["available_keys"] = NUM_KEYS;
  };
  benchmark::RegisterBenchmark(index_name, bench_func);
}

template <typename Index>
void bench_query(Index &index, const char * index_name, std::vector<int> & keys, std::vector<int> & queries) {
  auto bench_func = [&](::benchmark::State& state) {
    using T = typename Index::value_type;
    static size_t i_ = 0;
    T sum;
    index.bulk_load(keys.begin(), keys.end());
    for (auto _ : state) {
      auto result = index.find_next(queries[i_] ^ (sum & 0x01));
#ifndef NDEBUG
      auto lb = std::lower_bound(keys.begin(), keys.end(), queries[i_] ^ (sum & 0x01));
      if (result && lb == keys.end()) {
        std::cout << "Error: expected null optional; got: " << result.value() << std::endl;
        throw std::runtime_error("Incorrect result from find_next");
      }
      if (result != (lb == keys.end() ? -1 : *lb)) {
        std::cout << "Error: expected " << *lb << "; got: " << result.value() << std::endl;
        throw std::runtime_error("Incorrect result from find_next");
      }
#endif
      sum += result.value();
      i_ = (i_ == NUM_QUERIES - 1) ? 0 : i_ + 1;
    }
    state.counters["queried_keys"] = i_;
    state.counters["available_keys"] = NUM_KEYS;
    state.counters["dno"] = sum;
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

  std::shuffle(queries.begin(), queries.end(), std::mt19937{0});

  // Sorted keys for the queries
  std::vector<int> sorted_keys;
  std::copy(keys.begin(), keys.end(), std::back_inserter(sorted_keys));
  std::sort(sorted_keys.begin(), sorted_keys.end());

  DeLI::DeLI<int, low_bits> deli_index;
  bench_insert(deli_index, "DeLI Insert", keys, queries);
  bench_query(deli_index, "DeLI Query", sorted_keys, queries);

  // veb_tree veb_index;
  // bench_insert(veb_index, "VEB Insert", keys, queries);
  // bench_query(veb_index, "VEB Query", sorted_keys, queries);

  DeLI::RHT<int> rht_index(0, std::numeric_limits<int>::max());
  bench_insert(rht_index, "RHT Insert", keys, queries);
  bench_query(rht_index, "RHT Query", sorted_keys, queries);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
