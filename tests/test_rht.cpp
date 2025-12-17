#include <iostream>
#include "rht.h"
#include "test_index.h"

int main() {
    std::cout << "Running RHT Test" << std::endl;
    
    test_index<DeLI::RHT<int>>(0, 1 << 17, 1 << 20);

    // Test bulk loading
    {
        // Create a vector of random integers
        std::vector<int> data;
        const int N = 10000;
        for (int i = 0; i < N; ++i) {
            data.push_back(rand() % 100000);
        }
        check(!std::is_sorted(data.begin(), data.end()));

        DeLI::RHT<int> rht;
        bool loaded = rht.bulk_load(data.begin(), data.end(), 0, 1 << 17);
        if (!loaded) {
            std::cerr << "Bulk load failed due to high displacement." << std::endl;
            return 1;
        }

        // Test the sorted version
        std::sort(data.begin(), data.end());

        DeLI::RHT<int> rht_sorted;
        loaded = rht_sorted.bulk_load(data.begin(), data.end(), 0, 1 << 17);
        if (!loaded) {
            std::cerr << "Bulk load of sorted data failed due to high displacement." << std::endl;
            return 1;
        }

        check(rht == rht_sorted);
    }

    std::cout << "Test RHT passed!" << std::endl;
    return 0;
}