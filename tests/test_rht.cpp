#include <iostream>
#include "rht.h"
#include "test_index.h"

int main() {
    std::cout << "Running RHT Test" << std::endl;
    
    test_index<DeLI::RHT_wrapper<int>>(1024 * 1024);

    std::cout << "Test RHT passed!" << std::endl;
    return 0;
}