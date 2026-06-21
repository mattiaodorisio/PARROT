#include <iostream>
#include <vector>

#include "parrot.h"
#include "test_index.h"


template<bool dynamic, PARROT::RhtOptimization rht_opt, PARROT::TopLevelOptimization top_opt, size_t bits, size_t lg_buckets, size_t stride>
void test_bits() {
    using Index = PARROT::PARROT<dynamic, rht_opt, 2, 70, top_opt, uint_by_bits_t<bits>, lg_buckets, PARROT::NoPayload, bits>;
    std::cout << "SIMD width " << Index::rht_simd_width <<  std::endl;
    test_index<Index, bits, false>();
    if constexpr (bits > lg_buckets + stride) {
        test_bits<dynamic, rht_opt, top_opt, bits - stride, lg_buckets, stride>();
    }
}

template<bool dynamic, PARROT::RhtOptimization rhtOpt, PARROT::TopLevelOptimization topOpt>
void run() {
    test_bits<dynamic, rhtOpt, topOpt, 127, 1, 60>();
    test_bits<dynamic, rhtOpt, topOpt, 127, 1, 61>();
    test_bits<dynamic, rhtOpt, topOpt, 127, 2, 62>();
    test_bits<dynamic, rhtOpt, topOpt, 127, 8, 11>();
    test_bits<dynamic, rhtOpt, topOpt, 127, 16, 63>();
}

int main() {
    std::cout << "Running PARROT Test" << std::endl;

    run<false, PARROT::RhtOptimization::none, PARROT::TopLevelOptimization::none>();
    run<true, PARROT::RhtOptimization::none, PARROT::TopLevelOptimization::none>();
    run<true, PARROT::RhtOptimization::none, PARROT::TopLevelOptimization::bucket_index>();

    std::cout << "Test PARROT passed!" << std::endl;
    return 0;
}
