#include <iostream>

#include "veb.h"
#include "test_index.h"

int main() {
  std::cout << "Running VEB Test" << std::endl;
  
  test_index<veb_tree>();
  
  std::cout << "Test VEB passed!" << std::endl;
  return 0;
}
