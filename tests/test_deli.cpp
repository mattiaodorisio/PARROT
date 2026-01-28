#include <iostream>
#include <vector>

#include "deli.h"
#include "test_index.h"


template<bool dynamic, DeLI::RhtOptimization rht_opt, DeLI::TopLevelOptimization top_opt, size_t bits, size_t lg_buckets, size_t stride>
void test_bits() {
    using Index = DeLI::DeLI<dynamic, rht_opt, true, 70, top_opt, uint_by_bits_t<bits>, lg_buckets, bits>;
    test_index<Index, bits>();
    if constexpr (bits >= lg_buckets + stride) {
        test_bits<dynamic, rht_opt, top_opt, bits - stride, lg_buckets, stride>();
    }
}

template<bool dynamic, DeLI::RhtOptimization rhtOpt, DeLI::TopLevelOptimization topOpt>
void run() {
    test_bits<dynamic, rhtOpt, topOpt, 127, 0, 60>();
    test_bits<dynamic, rhtOpt, topOpt, 127, 1, 61>();
    test_bits<dynamic, rhtOpt, topOpt, 127, 2, 62>();
    test_bits<dynamic, rhtOpt, topOpt, 127, 8, 11>();
    test_bits<dynamic, rhtOpt, topOpt, 127, 16, 63>();
}

int main() {
    std::cout << "Running DeLI Test" << std::endl;

    run<false, DeLI::RhtOptimization::none, DeLI::TopLevelOptimization::none>();
    run<false, DeLI::RhtOptimization::none, DeLI::TopLevelOptimization::precompute>();
    run<true, DeLI::RhtOptimization::none, DeLI::TopLevelOptimization::none>();
    run<true, DeLI::RhtOptimization::none, DeLI::TopLevelOptimization::bucket_index>();

    std::cout << "Test DeLI passed!" << std::endl;
    return 0;
}
