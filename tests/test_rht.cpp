#include <iostream>
#include "rht.h"
#include "test_index.h"

template<size_t bits>
void test_bits() {
    using Index = DeLI::RHT<bits>;
    test_index<Index, bits>();
    if constexpr (bits>1) {
        test_bits<bits-1>();
    }
}

int main() {
  std::cout << "Running RHT Test" << std::endl;

  test_bits<6>();

  std::cout << "Test RHT passed!" << std::endl;
  return 0;
}