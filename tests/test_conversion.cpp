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
  for (int i = -1000; i <= 1000; ++i) {
    test_values.push_back(static_cast<T1>(i) / 10.0);
  }
  
  // Convert to uint
  for (const auto& val : test_values) {
    if constexpr (std::is_same<T1, double>::value && std::is_same<T2, uint64_t>::value) {
      converted_values.push_back(DeLI::utils::to_uint64(val));
    } else if constexpr (std::is_same<T1, float>::value && std::is_same<T2, uint32_t>::value) {
      converted_values.push_back(DeLI::utils::to_uint32(val));
    }
  }
  
  // Verify sorted order is preserved
  check(std::is_sorted(converted_values.begin(), converted_values.end()));
  
  // Convert back
  for (size_t i = 0; i < test_values.size(); ++i) {
    if constexpr (std::is_same<T1, double>::value && std::is_same<T2, uint64_t>::value) {
      check(test_values[i] == DeLI::utils::from_uint64(converted_values[i]));
    } else if constexpr (std::is_same<T1, float>::value && std::is_same<T2, uint32_t>::value) {
      check(test_values[i] == DeLI::utils::from_uint32(converted_values[i]));
    }
  }
}
int main() {
  std::cout << "Running data type Test" << std::endl;
  
  test<double, uint64_t>();
  test<float, uint32_t>();
  
  std::cout << "Test data type passed!" << std::endl;
  return 0;
}
