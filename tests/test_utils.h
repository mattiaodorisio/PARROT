#pragma once

#define check(condition) \
do { \
  if (!(condition)) { \
    std::cerr << "Check failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
    std::abort(); \
  } \
} while (0)


template <typename UInt>
constexpr UInt safe_shl(UInt value, unsigned shift) noexcept
{
  static_assert(std::is_unsigned_v<UInt>, "UInt must be an unsigned integer type");

  constexpr unsigned bits = std::numeric_limits<UInt>::digits;

  return shift < bits ? (value << shift) : UInt{0};
}

template <size_t Bits>
using uint_by_bits_t =
        std::conditional_t<(Bits <= 8),   uint8_t,
                std::conditional_t<(Bits <= 16),  uint16_t,
                        std::conditional_t<(Bits <= 32),  uint32_t,
                                std::conditional_t<(Bits <= 64),  uint64_t,
                                        __uint128_t>>>>;