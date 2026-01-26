#include <iostream>
#include <vector>

#include "deli.h"
#include "test_index.h"


template<size_t bits, size_t lg_buckets, size_t stride>
void test_bits() {
    using Index = DeLI::DeLI<uint_by_bits_t<bits>, lg_buckets, bits>;
    test_index<Index, bits>();
    if constexpr (bits >= lg_buckets + stride) {
        test_bits<bits - stride, lg_buckets, stride>();
    }
}

int main() {
    std::cout << "Running DeLI Test" << std::endl;

    test_bits<127, 0, 7>();
    test_bits<127, 8, 8>();
    test_bits<127, 16, 9>();

    std::cout << "Test DeLI passed!" << std::endl;
    return 0;
}
