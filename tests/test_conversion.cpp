#include <iostream>
#include <vector>
#include <algorithm>

#include "utils.h"
#include "test_utils.h"

/**
* This test ensures the utils library to convert floats/doubles to uint32/uint64 and back works correctly.
*/

template <typename T1, typename T2>
void test() {
  std::vector<T1> test_values;
  std::vector<T2> converted_values;
  
  // Generate test values
  if constexpr (std::is_signed<T1>::value) {
    for (int i = -1000; i <= 1000; ++i) {
      test_values.push_back(static_cast<T1>(i));
    }
  } else {
    for (unsigned int i = 0; i <= 2000; ++i) {
      test_values.push_back(static_cast<T1>(i));
    }
  }
  
  // Convert to uint
  for (const auto& val : test_values) {
    converted_values.push_back(DeLI::utils::to_uint<T1, T2>(val));
  }
  
  // Verify sorted order is preserved
  check(std::is_sorted(converted_values.begin(), converted_values.end()));
  
  // Convert back
  for (size_t i = 0; i < test_values.size(); ++i) {
    check(test_values[i] == (DeLI::utils::from_uint<T1, T2>(converted_values[i])));
  }
}
int main() {
  std::cout << "Running data type Test" << std::endl;
  
  test<double, uint64_t>();
  test<float, uint32_t>();
  test<int32_t, uint32_t>();
  test<int64_t, uint64_t>();
  test<unsigned int, uint32_t>();
  test<unsigned long, uint64_t>();
  
  std::cout << "Test data type passed!" << std::endl;
  return 0;
}
