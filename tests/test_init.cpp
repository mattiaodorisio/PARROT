#include <iostream>
#include <cstdint>

#include "parrot.h"
#include "test_utils.h"

int main() {
  std::cout << "Running PARROT Test 1" << std::endl;
  
  // Basic test - just ensure the library can be included and used
  PARROT::PARROT<true, PARROT::RhtOptimization::none, 2, 70, PARROT::TopLevelOptimization::none, uint32_t, 8> d;
  
  d.insert(42);
  
  std::cout << "Test 1 passed!" << std::endl;
  return 0;
}
