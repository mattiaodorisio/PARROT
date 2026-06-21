#include <iostream>
#include <vector>
#include <optional>

#include "parrot.h"

int main() {
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

    std::cout << "contains(7):  " << idx.contains(7) << "\n";   // true
    std::cout << "find_next(6): " << idx.find_next(6).value() << "\n";  // 7
    std::cout << "find_prev(7): " << idx.find_prev(7).value() << "\n";  // 5

    std::cout << "all keys: ";
    for (auto k : idx) {
        std::cout << k << " ";
    }
    std::cout << "\n";

    // Dynamic index supporting insertions and deletions
    using DynIndex = PARROT::PARROT<
        /*dynamic=*/true,
        PARROT::RhtOptimization::none,
        2, 70,
        PARROT::TopLevelOptimization::none,
        uint64_t,
        /*high_bits=*/16
    >;

    DynIndex dyn_idx;
    dyn_idx.insert(42);
    dyn_idx.insert(100);
    dyn_idx.remove(42);

    std::cout << "dynamic contains(42):  " << dyn_idx.contains(42) << "\n";   // false
    std::cout << "dynamic contains(100): " << dyn_idx.contains(100) << "\n";  // true

    return 0;
}
