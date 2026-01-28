

#include <vector>
#include <cstdint>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <set>
#include "rht.h"


#define XXH_PRIME64_2  0xC2B2AE3D27D4EB4FULL  /*!< 0b1100001010110010101011100011110100100111110101001110101101001111 */
#define XXH_PRIME64_3  0x165667B19E3779F9ULL  /*!< 0b0001011001010110011001111011000110011110001101110111100111111001 */

static uint64_t XXH64_avalanche(uint64_t hash)
{
    hash ^= hash >> 33;
    hash *= XXH_PRIME64_2;
    hash ^= hash >> 29;
    hash *= XXH_PRIME64_3;
    hash ^= hash >> 32;
    return hash;
}

static uint32_t skewwed(uint64_t hash) {
    uint64_t v=XXH64_avalanche(hash);
    return v>>32;
}

int main(int argc, char** argv) {
    size_t n=(1<<24) - (1<<21);
    typedef uint32_t T;
    std::vector<T> values;
    values.resize(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        values[i]= skewwed(i+1);
    }

    DeLI::RHT<T> rht(1<<24, 32 - 24, values);

    if(false) {
        std::cout << "starting validation" << std::endl;

        std::set<T> validate(values.begin(), values.end());
        std::uint64_t val_iterations = n << 1;

        for (std::uint64_t i = 1; i < val_iterations + 1; ++i) {
            T key = skewwed(i);
            auto res = rht.find_next(key);
            auto val = validate.upper_bound(key);
            if (res!=DeLI::RHT<T>::empty_v != (val != validate.end())) {
                exit(1);
            }
            if (res!=DeLI::RHT<T>::empty_v && (res != *val)) {
                exit(1);
            }
        }
    }
    std::cout<<"starting query"<<std::endl;

    std::uint64_t iterations = 1<<24;
    using clock = std::chrono::steady_clock;

    uint64_t sum=0;
    auto start = clock::now();
    for (std::uint64_t i = 1; i < iterations+1; ++i) {
        sum+= rht.find_next(skewwed(i));
    }
    auto end = clock::now();

    auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout<< static_cast<double>(elapsed_ns) / iterations <<" " <<sum<<std::endl;
}
