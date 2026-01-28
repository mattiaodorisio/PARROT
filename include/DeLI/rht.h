#pragma once

#include <vector>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <cstdint>
#include <experimental/simd>
#include <vector>
#include <cstdint>
#include <limits>

namespace DeLI {

    struct BitVector {
        std::vector<uint64_t> bits;
        static constexpr int W = 64;

        void resize(size_t n_bits) {
            bits.resize(n_bits / W + 1);
            set(n_bits);
        }

        uint64_t get(size_t i) const {
            return bits[i / W] & (uint64_t(1) << (i % W));
        }


        void set(size_t i) {
            bits[i / W] |= (uint64_t(1) << (i % W));
        }

        void clear(size_t i) {
            bits[i / W] &= ~(uint64_t(1) << (i % W));
        }

        size_t next_set(size_t pos) const {
            size_t w = pos / W;
            uint64_t word = bits[w];

            word &= (~uint64_t(0)) << (pos % W);

            uint64_t word2=bits[w+1];

            if ((word | word2) != 0) [[likely]]{
                return word!=0?w * W + __builtin_ctzll(word):(w+1) * W + __builtin_ctzll(word2);
            }
            w+=2;
            while (true) {
                if (bits[w] != 0)
                    return w * W + __builtin_ctzll(bits[w]);
                w++;
            }
        }
    };

    namespace stdx = std::experimental;

    template<typename T>
    class RHT {
    public:
        using sz_t = T;

        constexpr static T empty_v = std::numeric_limits<T>::min();
    private:
        using simd_t = stdx::native_simd<T>;
        constexpr static sz_t block_size = simd_t::size();
        std::vector<simd_t> table;
        BitVector non_empty_slots;

        // Sentinel values for empty slots
        constexpr static sz_t unrolled_blocks = 5;
        constexpr static float cluster_safety_factor = 1.1;

        sz_t key_shift;

    public:

        RHT(sz_t slots, sz_t key_shift, std::vector<T> keys) : key_shift(key_shift) {
            assert(key_shift < sizeof(T) * 8);
            sz_t regular_blocks = 1 + float(slots) * cluster_safety_factor / float(block_size);
            table.resize(regular_blocks + unrolled_blocks, simd_t(empty_v));
            non_empty_slots.resize(table.size() * block_size - 1);

            std::sort(keys.begin(), keys.end());
            sz_t nextFreeSlot = 0;
            for (T k: keys) {
                sz_t slot = get_slot(k);
                slot = std::max(nextFreeSlot, slot);
                table[slot / block_size][slot % block_size] = k;
                non_empty_slots.set(slot);
                nextFreeSlot = slot + 1;
            }
        }

        sz_t get_slot(T key) const {
            return key >> key_shift;
        }

        T find_next(T key) const {
            assert(key != empty_v);

            sz_t probe = get_slot(key);
            /*do {
                T chunk1 = table[probe/block_size][probe%block_size]; probe++;
                if (chunk1 > key) {
                    return chunk1;
                }
            } while(probe < table.size()*block_size);
            return empty_v;*/

            while (probe < table.size()*block_size) {
                T chunk1 = table[probe/block_size][probe%block_size] - (key + 1); probe++;
                T chunk2 = table[probe/block_size][probe%block_size] - (key + 1); probe++;
                //T chunk3 = table[probe/block_size][probe%block_size] - (key + 1); probe++;
                T chunk = std::min({ chunk2, chunk1});
                T min_val = chunk + (key +1);
                if (min_val > key) [[likely]]{
                    return min_val;
                }
            }
            return empty_v;
            /*sz_t slot=get_slot(key);
            if (non_empty_slots.get(slot)) {
                sz_t probe = slot / block_size;
                assert(probe < table.size() - unrolled_blocks);
                do {
                    simd_t chunk1 = table[probe++] - (key + 1);
                    simd_t chunk2 = table[probe++] - (key + 1);
                    simd_t chunk = stdx::min(chunk2, chunk1);
                    T min_val = stdx::hmin(chunk) + (key + 1);
                    if (min_val > key) [[likely]] {
                        return min_val;
                    }
                } while (probe < table.size() - unrolled_blocks);
                return empty_v;
            } else {
                slot = non_empty_slots.next_set(slot);
                return table[slot/block_size][slot%block_size];
            }*/
        }
    };


}