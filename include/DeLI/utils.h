#pragma once

#include <stdint.h>
#include <cstring>
#include <type_traits>

namespace DeLI::utils {
  
  // credit http://stereopsis.com/radix.html
  
  // Generic template functions - to be specialized for specific types
  // UIntType can only be uint32_t or uint64_t
  template<typename T, typename UIntType>
  inline UIntType to_uint(const T value);
  
  template<typename T, typename UIntType>
  inline T from_uint(UIntType x);
  
  // Specializations for double -> uint64_t
  template<>
  inline uint64_t to_uint<double, uint64_t>(const double d) {
    uint64_t r;
    std::memcpy(&r, &d, sizeof(double));
    uint64_t mask = -int64_t(r >> 63) | 0x8000000000000000;
    return r ^ mask;
  }
  
  template<>
  inline double from_uint<double, uint64_t>(uint64_t x) {
    uint64_t mask = ((x >> 63) - 1) | 0x8000000000000000;
    uint64_t r = x ^ mask;
    double res;
    std::memcpy(&res, &r, sizeof(double));
    return res;
  }
  
  // Specializations for float -> uint32_t
  template<>
  inline uint32_t to_uint<float, uint32_t>(const float f) {
    uint32_t r;
    std::memcpy(&r, &f, sizeof(float));
    uint32_t mask = -int32_t(r >> 31) | 0x80000000;
    return r ^ mask;
  }
  
  template<>
  inline float from_uint<float, uint32_t>(uint32_t f) {
    uint32_t mask = ((f >> 31) - 1) | 0x80000000;
    uint32_t r = f ^ mask;
    float res;
    std::memcpy(&res, &r, sizeof(float));
    return res;
  }

  // int64_t -> uint64_t
  template<>
  inline uint64_t to_uint<int64_t, uint64_t>(const int64_t value) {
    return static_cast<uint64_t>(value) + static_cast<uint64_t>(1LL << 63);
  }

  template<>
  inline int64_t from_uint<int64_t, uint64_t>(uint64_t x) {
    return static_cast<int64_t>(x - static_cast<uint64_t>(1LL << 63));
  }

  // uint64_t -> uint64_t (identity)
  template<>
  inline uint64_t to_uint<uint64_t, uint64_t>(const uint64_t value) {
    return value;
  }

  template<>
  inline uint64_t from_uint<uint64_t, uint64_t>(uint64_t x) {
    return x;
  }

  // int32_t -> uint32_t
  template<>
  inline uint32_t to_uint<int32_t, uint32_t>(const int32_t value) {
    return static_cast<uint32_t>(value) + static_cast<uint32_t>(1 << (sizeof(int32_t) * 8 - 1));
  }

  template<>
  inline int32_t from_uint<int32_t, uint32_t>(uint32_t x) {
    return static_cast<int32_t>(x - static_cast<uint32_t>(1 << (sizeof(int32_t) * 8 - 1)));
  }

  // uint32_t -> uint32_t (identity)
  template<>
  inline uint32_t to_uint<uint32_t, uint32_t>(const uint32_t value) {
    return value;
  }

  template<>
  inline uint32_t from_uint<uint32_t, uint32_t>(uint32_t x) {
    return x;
  }

} // namespace DeLI::utils
