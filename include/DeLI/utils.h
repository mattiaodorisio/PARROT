#pragma once

#include <stdint.h>
#include <cstring>

namespace DeLI::utils {

    // credit http://stereopsis.com/radix.html
    
    inline uint64_t to_uint64(const double d) {
        uint64_t r;
        std::memcpy(&r, &d, sizeof(double));
        uint64_t mask = -int64_t(r >> 63) | 0x8000000000000000;
        return r ^ mask;
    }

    inline double from_uint64(uint64_t x)
    {
        uint64_t mask = ((x >> 63) - 1) | 0x8000000000000000;
        uint64_t r = x ^ mask;
        double res;
        std::memcpy(&res, &r, sizeof(double));
        return res;
    }

    inline uint32_t to_uint32(const float f) {
        uint32_t r;
        std::memcpy(&r, &f, sizeof(float));
        uint32_t mask = -int32_t(r >> 31) | 0x80000000;
        return r ^ mask;
    }

    inline float from_uint32(uint32_t f)
    {
        uint32_t mask = ((f >> 31) - 1) | 0x80000000;
        uint32_t r = f ^ mask;
        float res;
        std::memcpy(&res, &r, sizeof(float));
        return res;
    }

} // namespace DeLI::utils
