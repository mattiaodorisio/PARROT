#include <iostream>
#include "rht.h"
#include "test_index.h"

template<size_t stride, bool dynamic, size_t bits, DeLI::RhtOptimization opt>
void test_bits() {
    using Index = DeLI::RHT<dynamic, bits, opt>;
    test_index<Index, bits>();
    if constexpr (bits >= stride) {
        test_bits<stride, dynamic, bits - stride, opt>();
    }
}

int main() {
    std::cout << "Running RHT Test" << std::endl;

    test_bits<8, true, 127, DeLI::RhtOptimization::slot_index>();
    test_bits<9, true, 127, DeLI::RhtOptimization::none>();
    test_bits<10, false, 127, DeLI::RhtOptimization::gap_fill_predecessor>();
    test_bits<11, false, 127, DeLI::RhtOptimization::gap_fill_successor>();
    test_bits<12, false, 127, DeLI::RhtOptimization::gap_fill_both>();
    test_bits<13, false, 127, DeLI::RhtOptimization::slot_index>();
    test_bits<14, false, 127, DeLI::RhtOptimization::none>();

    std::cout << "Test RHT passed!" << std::endl;
    return 0;
}