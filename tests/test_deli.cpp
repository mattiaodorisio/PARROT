#include <iostream>

#include "deli.h"
#include "test_index.h"

int main() {
    std::cout << "Running DeLI Test" << std::endl;
    
    test_index<DeLI::DeLI<int, 22>>();

    std::cout << "Test DeLI passed!" << std::endl;
    return 0;
}
