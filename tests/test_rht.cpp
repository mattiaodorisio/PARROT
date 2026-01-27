#include <iostream>
#include "rht.h"
#include "test_index.h"

template<bool dynamic, size_t bits>
void test_bits() {
    using Index = DeLI::RHT<dynamic, bits>;
    test_index<Index, bits>();
    if constexpr (bits>1) {
        test_bits<dynamic, bits-1>();
    }
}

int main() {
  std::cout << "Running RHT Test" << std::endl;

  test_bits<true, 127>();
  test_bits<false, 127>();

  std::cout << "Test RHT passed!" << std::endl;
  return 0;
}