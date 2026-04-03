#include <iostream>
#include "rht.h"
#include "test_index.h"

template<size_t stride, bool dynamic, size_t bits, DeLI::RhtOptimization opt, size_t simd>
void test_bits() {
    using Index = DeLI::RHT<dynamic, bits, opt, simd, 50>;
    DeLI::utils::print_simd_info<typename Index::Tvec>();
    test_index<Index, bits>();
    if constexpr (bits > stride) {
        test_bits<stride, dynamic, bits - stride, opt, simd>();
    }
}

template<size_t simd>
void dispatch() {
    if constexpr (simd == 0) {
        test_bits<21, true, 127, DeLI::RhtOptimization::slot_index, simd>();
    }
    test_bits<22, true, 16, DeLI::RhtOptimization::none, simd>();
    test_bits<23, false, 127, DeLI::RhtOptimization::gap_fill_predecessor, simd>();
    test_bits<24, false, 127, DeLI::RhtOptimization::gap_fill_successor, simd>();
    test_bits<25, false, 127, DeLI::RhtOptimization::gap_fill_both, simd>();
    if constexpr (simd == 0) {
        test_bits<26, false, 127, DeLI::RhtOptimization::slot_index, simd>();
    }
    test_bits<27, false, 127, DeLI::RhtOptimization::none, simd>();
}

int main() {
    std::cout << "Running RHT Test" << std::endl;

    dispatch<0>();
    dispatch<1>();
    dispatch<2>();
    dispatch<4>();
    dispatch<5>();

    std::cout << "Test RHT passed!" << std::endl;
    return 0;
}