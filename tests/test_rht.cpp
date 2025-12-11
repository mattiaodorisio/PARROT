#include <iostream>
#include "rht.h"
#include "test_index.h"

int main() {
    std::cout << "Running RHT Test" << std::endl;
    
    test_index<DeLI::RHT<int>>(1 << 26, std::numeric_limits<int>::max());

    std::cout << "Test RHT passed!" << std::endl;
    return 0;
}