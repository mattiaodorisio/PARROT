#include <iostream>
#include <cstdint>

#include "deli.h"
#include "test_utils.h"

int main() {
  std::cout << "Running DeLI Test 1" << std::endl;
  
  // Basic test - just ensure the library can be included and used
  DeLI::DeLI<true, DeLI::RhtOptimization::none, 2, 70, DeLI::TopLevelOptimization::none, uint32_t, 8> d;
  
  d.insert(42);
  
  std::cout << "Test 1 passed!" << std::endl;
  return 0;
}
