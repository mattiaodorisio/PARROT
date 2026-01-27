#include <iostream>
#include <vector>

#include "deli.h"
#include "test_index.h"


template<bool dynamic, DeLI::RhtOptimization opt, size_t bits, size_t lg_buckets, size_t stride>
void test_bits() {
    using Index = DeLI::DeLI<dynamic, opt, uint_by_bits_t<bits>, lg_buckets, bits>;
    test_index<Index, bits>();
    if constexpr (bits >= lg_buckets + stride) {
        test_bits<dynamic, opt, bits - stride, lg_buckets, stride>();
    }
}

template<bool dynamic, DeLI::RhtOptimization opt>
void run() {
    test_bits<dynamic, opt, 127, 1, 7>();
    test_bits<dynamic, opt, 127, 8, 8>();
    test_bits<dynamic, opt, 127, 16, 9>();
}

int main() {
    std::cout << "Running DeLI Test" << std::endl;

    run<true, DeLI::RhtOptimization::none>();
    run<false, DeLI::RhtOptimization::gap_fill_both>();

    std::cout << "Test DeLI passed!" << std::endl;
    return 0;
}
