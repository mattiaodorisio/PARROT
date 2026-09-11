# PARROT

**PAR**titioned **RO**bin Hood **T**able.

[PARROT](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ESA.2026.15) is a learned predecessor data structure. It learns the distribution of the input keys to accelerate operations such as insert, delete, successor, and predecessor queries. Unlike other learned indexes, its simple two-level design, a top-level routing table and bottom-level Robin hash tables, allows PARROT to achieve expected constant-time operations under smoothed analysis.

It partitions keys by their `b` most significant bits into buckets, each stored as a Robin Hood Table (RHT) with key-scaled positions. Under smoothed analysis, all operations run in expected constant time.

**Highlights:**
- Header-only C++23 library
- Supports static (bulk-load) and dynamic (insert/delete) modes
- Successor and predecessor queries
- Optional key-value storage
- SIMD-accelerated search

## Build

Requires C++23 and CMake ≥ 3.12.

```bash
git clone https://github.com/mattiaodorisio/PARROT
cd PARROT
cmake -B build
cmake --build build
ctest --test-dir build
```

PARROT is a header-only library. To use it in another CMake project, add it as a subdirectory or install it and use `find_package(PARROT)`.

## Usage

```cpp
#include "parrot.h"

// Static index (bulk-load only), 32-bit keys, 8 top-level bits, no payload
using Index = PARROT::PARROT<
    /*dynamic=*/false,
    PARROT::RhtOptimization::none,
    /*rht_simd_unrolled=*/2,
    /*rht_max_load_perc=*/70,
    PARROT::TopLevelOptimization::bucket_index,
    uint32_t,
    /*high_bits=*/8
>;

std::vector<uint32_t> keys = {1, 5, 7, 42, 100};
// keys must be sorted
Index idx;
idx.bulk_load(keys.begin(), keys.end());

idx.contains(7);           // true
idx.find_next(6);          // std::optional<uint32_t>{7}  (successor, >=)
idx.find_prev(7);          // std::optional<uint32_t>{5}  (predecessor, <)

for (auto k : idx) { /* iterate in sorted order */ }
```

For a dynamic index supporting insertions and deletions:

```cpp
using DynIndex = PARROT::PARROT<
    /*dynamic=*/true,
    PARROT::RhtOptimization::none,
    2, 70,
    PARROT::TopLevelOptimization::none,
    uint64_t,
    /*high_bits=*/16
>;

DynIndex idx;
idx.insert(42);
idx.insert(100);
idx.remove(42);
```

Key-value pairs are supported by setting the `PayloadT` template parameter:

```cpp
using MapIndex = PARROT::PARROT<
    true,
    PARROT::RhtOptimization::none,
    2, 70,
    PARROT::TopLevelOptimization::none,
    uint32_t,
    /*high_bits=*/8,
    std::string   // payload type
>;
```

## Template Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `dynamic` | `bool` | Enable `insert` / `remove` |
| `rht_opt` | `RhtOptimization` | RHT variant: `none`, `slot_index`, `gap_fill_predecessor`, `gap_fill_successor`, `gap_fill_both` (gap-fill variants are static only) |
| `rht_simd_unrolled` | `size_t` | SIMD unrolling factor for RHT search; 0 disables SIMD |
| `rht_max_load_perc` | `size_t` | Maximum RHT load factor (%) before resize |
| `opt` | `TopLevelOptimization` | `none` or `bucket_index` (bitvector for O(log log n) cross-bucket queries) |
| `T` | type | Key type (integers, floats, …) |
| `high_bits` | `unsigned int` | Bits used for top-level routing (controls number of buckets = 2^high_bits) |
| `PayloadT` | type | Value type; defaults to `NoPayload` for a set |
| `value_bits` | `unsigned int` | Effective key width in bits; defaults to `sizeof(T)*8` |
| `top_structure` | template | Top-level map; defaults to `std::unordered_map`. We highly recommend [`ankerl::unordered_dense`](https://github.com/martinus/unordered_dense) for better performance |

## API

| Method | Description |
|--------|-------------|
| `bulk_load(begin, end)` | Load a sorted range (keys or pairs) |
| `insert(key [, payload])` | Insert a key (dynamic only) |
| `remove(key)` | Remove a key (dynamic only) |
| `contains(key)` | Membership test |
| `find_next(key)` | Successor: first element ≥ key |
| `find_prev(key)` | Predecessor: first element < key |
| `find_next_iter(key)` | Successor as iterator |
| `find_prev_iter(key)` | Predecessor as iterator |
| `begin()` / `end()` | Forward iteration in sorted order |
| `size()` | Number of stored keys |
| `size_in_bytes()` | Total memory usage in bytes |
| `clear()` | Remove all elements (dynamic only) |

## Benchmark

The companion benchmark repository is at https://github.com/mattiaodorisio/PARROT-testbed.

## Paper

For more details on PARROT, see the [paper](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ESA.2026.15).
