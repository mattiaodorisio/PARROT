#include <iostream>

#include "bitvector.h"
#include "test_index.h"

int main() {
  std::cout << "Running bitvector Test" << std::endl;

  using Index = PARROT::TwoLevelBitvector;
  constexpr int bits = 20;
  test_index<Index, bits, true>();

  std::cout << "Test bitvector passed!" << std::endl;
  return 0;
}
