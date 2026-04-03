#include <cstdint>
#include <iostream>

#include "rht.h"
#include "test_index.h"

template<size_t stride, bool dynamic, size_t bits, DeLI::RhtOptimization opt, size_t simd>
void test_bits_payloads() {
    using Index = DeLI::RHT<dynamic, bits, opt, simd, 70, uint64_t>;
    DeLI::utils::print_simd_info<typename Index::Tvec>();
    test_index_payload<Index, bits>();
    if constexpr (bits > stride) {
        test_bits_payloads<stride, dynamic, bits - stride, opt, simd>();
    }
}

template<size_t simd>
void dispatch_payload() {
    if constexpr (simd == 0) {
        test_bits_payloads<21, true, 127, DeLI::RhtOptimization::slot_index, simd>();
    }
    test_bits_payloads<22, true, 16, DeLI::RhtOptimization::none, simd>();
    test_bits_payloads<23, false, 127, DeLI::RhtOptimization::gap_fill_predecessor, simd>();
    test_bits_payloads<24, false, 127, DeLI::RhtOptimization::gap_fill_successor, simd>();
    test_bits_payloads<25, false, 127, DeLI::RhtOptimization::gap_fill_both, simd>();
    if constexpr (simd == 0) {
        test_bits_payloads<26, false, 127, DeLI::RhtOptimization::slot_index, simd>();
    }
    test_bits_payloads<27, false, 127, DeLI::RhtOptimization::none, simd>();

    test_bits_payloads<60, false, 127, DeLI::RhtOptimization::none, simd>();
    test_bits_payloads<61, false, 127, DeLI::RhtOptimization::none, simd>();
    test_bits_payloads<62, false, 127, DeLI::RhtOptimization::none, simd>();
    test_bits_payloads<11, false, 127, DeLI::RhtOptimization::none, simd>();
    test_bits_payloads<63, false, 127, DeLI::RhtOptimization::none, simd>();

    if constexpr (simd == 2) {
        using IndexBits6Dynamic = DeLI::RHT<true, 6, DeLI::RhtOptimization::none, 2, 70, uint64_t>;
        using IndexBits6Static = DeLI::RHT<false, 6, DeLI::RhtOptimization::none, 2, 70, uint64_t>;
        test_index_payload<IndexBits6Dynamic, 6>();
        test_index_payload<IndexBits6Static, 6>();
    }
}

int main() {
    std::cout << "Running RHT payload tests" << std::endl;

    dispatch_payload<0>();
    dispatch_payload<1>();
    dispatch_payload<2>();
    dispatch_payload<5>();

    std::cout << "RHT payload tests passed!" << std::endl;
    return 0;
}
