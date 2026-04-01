#include <vector>
#include <string>
#include <iostream>
#include <set>
#include <random>
#include <chrono>

#include "deli.h"
#include "veb.h"
#include "rht.h"
#define XXH_PRIME64_2  0xC2B2AE3D27D4EB4FULL  /*!< 0b1100001010110010101011100011110100100111110101001110101101001111 */
#define XXH_PRIME64_3  0x165667B19E3779F9ULL  /*!< 0b0001011001010110011001111011000110011110001101110111100111111001 */

static uint64_t XXH64_avalanche(uint64_t hash) {
    hash ^= hash >> 33;
    hash *= XXH_PRIME64_2;
    hash ^= hash >> 29;
    hash *= XXH_PRIME64_3;
    hash ^= hash >> 32;
    return hash;
}

constexpr bool test = false;

using T = uint32_t;

template<typename Index>
void bench_query(Index &index, std::vector<uint64_t> &keys, std::vector<uint64_t> &queries) {
    std::set<T> test_set;
    T sum = 0;
    if constexpr (test) {
        test_set.insert(keys.begin(), keys.end());
    }
    index.bulk_load(keys.begin(), keys.end());

    using clock = std::chrono::steady_clock;
    auto start = clock::now();
    for (auto q:queries) {
        auto result = index.find_next(q);
        if constexpr (test) {
            auto it_next = test_set.lower_bound(q);
            if (it_next != test_set.end()) {
                T expected_next = *it_next;
                if(result != expected_next){
                    exit(1);
                }
            } else {
                if(result != std::nullopt) {
                    exit(1);
                }
            }
        }
        sum += result.value_or(0);
    }
    auto end = clock::now();

    auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << static_cast<double>(elapsed_ns) / queries.size() << " " << sum << std::endl;
}

int main(int argc, char **argv) {
    constexpr size_t NUM_QUERIES = 1 << 20;
    constexpr size_t bits = 10;
    constexpr uint64_t mask = (uint64_t(1)<<bits)-1;
    // Generate a vector of keys
    for (uint64_t log_keys = 7; log_keys < 24; ++log_keys) {
        std::cout<<log_keys<<" ";
        uint64_t NUM_KEYS = uint64_t(1)<<log_keys;
        std::vector<uint64_t > keys(NUM_KEYS), queries(NUM_QUERIES);
        for (size_t i = 0; i < NUM_KEYS; ++i) {
            keys[i] = XXH64_avalanche(i) & mask;
        }
        for (size_t i = 0; i < NUM_QUERIES; ++i) {
            queries[i] = XXH64_avalanche(~i) & mask;
        }

        // Sorted keys for the queries
        std::vector<uint64_t> sorted_keys;
        std::copy(keys.begin(), keys.end(), std::back_inserter(sorted_keys));
        std::sort(sorted_keys.begin(), sorted_keys.end());

        using Index = DeLI::RHT<true, bits, DeLI::RhtOptimization::none, 4, 50>;
        //using Index = DeLI::DeLI<true, DeLI::RhtOptimization::none, 5, 50, DeLI::TopLevelOptimization::none, T , 16, bits >;
        Index rht_index;
        //DeLI::utils::print_simd_info<Index::Tvec >();

        bench_query(rht_index, sorted_keys, queries);
    }
}
