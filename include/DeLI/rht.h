#pragma once

#include <vector>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <cstdint>
#include <experimental/simd>

namespace DeLI {
    namespace stdx = std::experimental;

    template<typename T>
    class RHT {
    public:
        using sz_t = T;

    private:
        using simd_t = stdx::native_simd<T>;
        constexpr static sz_t block_size = simd_t::size();
        std::vector<simd_t> table;

        // Sentinel values for empty slots
        constexpr static T empty_v = std::numeric_limits<T>::min();
        constexpr static sz_t unrolled_blocks = 5;
        constexpr static float cluster_safety_factor = 1.1;

        sz_t key_shift;

    public:

        RHT(sz_t slots, sz_t key_shift, std::vector<T> keys) : key_shift(key_shift) {
            assert(key_shift < sizeof(T) * 8);
            sz_t regular_blocks = 1 + float(slots) * cluster_safety_factor / float(block_size);
            table.resize(regular_blocks + unrolled_blocks, simd_t(empty_v));

            std::sort(keys.begin(), keys.end());
            sz_t current_block = 0;
            sz_t in_block = 0;
            for (T k: keys) {
                sz_t slot = get_slot(k);
                if(slot/block_size > current_block) {
                    in_block=0;
                    current_block = slot/block_size;
                }
                table[current_block][in_block] = k;
                in_block++;
                if(in_block==block_size) {
                    in_block=0;
                    current_block++;
                }
            }
        }

        sz_t get_slot(T key) const {
            return key >> key_shift;
        }

        std::optional<T> find_next(T key) const {
            assert(key != empty_v);

            /*sz_t probe = get_slot(key);
            while (probe < table.size()*block_size) {
                T chunk1 = table[probe/block_size][probe%block_size] - (key + 1); probe++;
                T chunk2 = table[probe/block_size][probe%block_size] - (key + 1); probe++;
                T chunk3 = table[probe/block_size][probe%block_size] - (key + 1); probe++;
                T chunk = std::min({ chunk3,chunk2, chunk1});
                T min_val = chunk + (key +1);
                if (min_val > key) [[likely]]{
                    return min_val;
                }
            }
            return std::optional<T>();*/


            sz_t probe = get_slot(key) / block_size;
            assert(probe < table.size() - unrolled_blocks);
            do {
                simd_t chunk1 = table[probe++] - (key + 1);
                simd_t chunk2 = table[probe++] - (key + 1);
                simd_t chunk3 = table[probe++] - (key + 1);
                simd_t chunk = stdx::min(chunk3, stdx::min(chunk2, chunk1));
                T min_val = stdx::hmin(chunk) + (key + 1);
                if (min_val > key) [[likely]] {
                    return min_val;
                }
            } while (probe < table.size() - unrolled_blocks);
            return std::optional<T>();
        }
    };
}