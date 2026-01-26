#include <iostream>
#include <vector>

#include "deli.h"
#include "test_index.h"


template<size_t bits, size_t lg_buckets>
void test_bits() {
    using Index = DeLI::DeLI<uint64_t, lg_buckets, bits>;
    test_index<Index, bits>();
    if constexpr (bits > lg_buckets) {
        test_bits<bits - 1, lg_buckets>();
    }
}

int main() {
    std::cout << "Running DeLI Test" << std::endl;

    //test_bits<63, 0>();
    test_bits<63, 8>();
    test_bits<63, 16>();

    std::cout << "Test DeLI passed!" << std::endl;
    return 0;
}
