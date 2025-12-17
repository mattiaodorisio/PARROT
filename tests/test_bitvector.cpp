#include <iostream>
#include <vector>
#include <algorithm>

#include "bitvector.h"
#include "test_utils.h"

int main() {
  std::cout << "Running bitvector Test" << std::endl;
  
  DeLI::bitvector bv(130);
  
  // Set some bits
  bv.set(0);
  bv.set(65);
  bv.set(129);
  
  // Check if bits are set correctly
  check(bv.get(0) == true);
  check(bv.get(1) == false);
  check(bv.get(65) == true);
  check(bv.get(128) == false);
  check(bv.get(129) == true);
  
  // Find next set bits
  check(bv.find_next_set(0) == 0);
  check(bv.find_next_set(1) == 65);
  check(bv.find_next_set(66) == 129);
  check(bv.find_next_set(130) == 130); // No set bit found
  
  // Clear a bit and check
  bv.clear(65);
  check(bv.get(65) == false);
  check(bv.find_next_set(1) == 129);
  bv.free_memory(); 
  
  // Test the xor bitvector
  std::vector<int> data(100000);
  data[12345] = 1;
  data[56789] = 1;
  data[67890] = 1;
  DeLI::xor_bitvector<int, 0> bv_xor;
  bv_xor.build(data.begin(), data.end());
  check(bv_xor.get(12345) == true);
  check(bv_xor.get(56789) == true);
  check(bv_xor.get(67890) == true);
  check(bv_xor.get(0) == false);
  
  check(bv_xor.find_next_set(0) == 12345);
  check(bv_xor.find_next_set(12345) == 12345);
  check(bv_xor.find_next_set(20000) == 56789);
  check(bv_xor.find_next_set(56790) == 67890);
  check(bv_xor.find_next_set(67891) == 100000);
  
  std::cout << "Test bitvector passed!" << std::endl;
  return 0;
}
