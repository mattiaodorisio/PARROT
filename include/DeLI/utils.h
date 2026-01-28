#pragma once

#include <stdint.h>
#include <cstring>
#include <type_traits>
#include <limits>
#include <string>
#include <algorithm>

namespace DeLI::utils {
  
  // credit http://stereopsis.com/radix.html
  
  // Generic template functions - to be specialized for specific types
  // UIntType can only be uint32_t or uint64_t
  template<typename T, typename UIntType>
  inline UIntType to_uint(const T value) {
      return UIntType(value);
  }
  
  template<typename T, typename UIntType>
  inline T from_uint(UIntType x) {
      return T(x);
  }
  
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


    template <typename UInt>
    constexpr UInt safe_shl(UInt value, unsigned shift) noexcept
    {
      static_assert(std::is_unsigned_v<UInt>, "UInt must be an unsigned integer type");

      constexpr unsigned bits = std::numeric_limits<UInt>::digits;

      return shift < bits ? (value << shift) : UInt{0};
    }

    template <typename UInt>
    constexpr UInt safe_shr(UInt value, unsigned shift) noexcept
    {
      static_assert(std::is_unsigned_v<UInt>, "UInt must be an unsigned integer type");

      constexpr unsigned bits = std::numeric_limits<UInt>::digits;

      return shift < bits ? (value >> shift) : UInt{0};
    }

    template <typename UInt>
    std::string to_string_unsigned(UInt value)
    {
      static_assert(std::is_unsigned_v<UInt>);

      if (value == 0)
        return "0";

      std::string result;
      while (value > 0)
      {
        result.push_back(char('0' + (value % 10)));
        value /= 10;
      }

      std::reverse(result.begin(), result.end());
      return result;
    }

    template <typename UInt>
    std::string to_string(UInt value)
    {
      if constexpr (std::is_same_v<UInt, __uint128_t>)
        return to_string_unsigned(value);
      else
        return std::to_string(value);
    }

    template<typename T, std::size_t Alignment = 64>
    struct AlignedAllocator {
        using value_type = T;

        AlignedAllocator() noexcept = default;

        template<class U>
        constexpr AlignedAllocator(const AlignedAllocator<U, Alignment> &) noexcept {}

        T *allocate(std::size_t n) {
          if (n == 0) return nullptr;
          void *ptr = nullptr;
#if __cpp_aligned_new >= 201606L
          ptr = ::operator new(n * sizeof(T), std::align_val_t(Alignment));
#else
          if (posix_memalign(&ptr, Alignment, n * sizeof(T))) ptr = nullptr;
#endif
          if (!ptr) throw std::bad_alloc();
          return static_cast<T *>(ptr);
        }

        void deallocate(T *p, std::size_t) noexcept {
#if __cpp_aligned_new >= 201606L
          ::operator delete(p, std::align_val_t(Alignment));
#else
          std::free(p);
#endif
        }

        // === Required for older STL / rebind ===
        template<typename U>
        struct rebind {
            using other = AlignedAllocator<U, Alignment>;
        };
    };

// comparison operators
    template<typename T1, typename T2, std::size_t A>
    bool operator==(const AlignedAllocator<T1, A> &, const AlignedAllocator<T2, A> &) { return true; }

    template<typename T1, typename T2, std::size_t A>
    bool operator!=(const AlignedAllocator<T1, A> &, const AlignedAllocator<T2, A> &) { return false; }

// alias
    template<typename T, std::size_t Alignment = 64>
    using AlignedVector = std::vector<T, AlignedAllocator<T, Alignment>>;


    template <size_t Bits>
    using uint_by_bits_t =
            std::conditional_t<(Bits <= 8),   uint8_t,
                    std::conditional_t<(Bits <= 16),  uint16_t,
                            std::conditional_t<(Bits <= 32),  uint32_t,
                                    std::conditional_t<(Bits <= 64),  uint64_t,
                                            __uint128_t>>>>;



} // namespace DeLI::utils

