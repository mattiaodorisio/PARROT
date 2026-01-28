#pragma once


#include <vector>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <cassert>

namespace DeLI {
    class TwoLevelBitvector {
    public:
        using word_t = uint64_t;
        static constexpr unsigned WORD_BITS = sizeof(word_t) * 8;

        // Construct with capacity bits (will allocate enough words).
        explicit TwoLevelBitvector(size_t capacity_bits = 0) {
            resize(capacity_bits);
        }

        // Resize to at least capacity_bits (bits). Existing bits are preserved when increasing.
        void resize(size_t capacity_bits) {
            size_t needed_words = (capacity_bits + WORD_BITS - 1) / WORD_BITS;
            if (needed_words <= data_words()) return;
            // resize data words
            data_.resize(needed_words, 0);
            // ensure top words cover needed_words words (top has one bit per data-word)
            size_t needed_top_bits = needed_words;
            size_t needed_top_words = (needed_top_bits + WORD_BITS - 1) / WORD_BITS;
            top_.resize(needed_top_words, 0);
        }

        // set bit at pos (0-based)
        void insert(size_t pos) {
            ensure_capacity_for(pos);
            size_t widx = pos >> 6;
            unsigned b = pos & 63;
            word_t old = data_[widx];
            data_[widx] |= (word_t(1) << b);
            if (old == 0) // transition 0 -> non-zero: set top bit
                set_top_bit(widx);
        }

        // clear bit at pos
        void remove(size_t pos) {
            if (pos / WORD_BITS >= data_words()) return; // out of range: already zero
            size_t widx = pos >> 6;
            unsigned b = pos & 63;
            data_[widx] &= ~(word_t(1) << b);
            if (data_[widx] == 0) // transition to zero: clear top bit
                clear_top_bit(widx);
        }

        // test bit at pos
        bool contains(size_t pos) const {
            if (pos / WORD_BITS >= data_words()) return false;
            size_t widx = pos >> 6;
            unsigned b = pos & 63;
            return (data_[widx] >> b) & 1ULL;
        }

        // predecessor: greatest set bit <= pos. returns std::nullopt if none.
        std::optional<size_t> find_prev(size_t pos) const {
            assert(pos <= (data_words() * WORD_BITS) - 1);
            size_t widx = pos >> 6;
            unsigned b = pos & 63;

            // mask bits <= b in the same word
            word_t w = data_[widx] & ((b == 63) ? ~word_t(0) : ((word_t(1) << (b + 1)) - 1ULL));
            if (w) [[likely]] {
                unsigned msb = msb_index(w);
                return (widx << 6) + msb;
            }

            // find previous non-empty data-word by checking top level bits
            // look for set top bits for word indices < widx
            if (widx == 0) return std::nullopt;
            size_t top_bit_idx = widx; // top bit index we need strictly < widx
            size_t top_word_idx = (top_bit_idx - 1) >> 6;
            unsigned in_word_bit = (top_bit_idx - 1) & 63;

            // mask top word bits <= in_word_bit
            word_t topw =
                    top_[top_word_idx] & ((in_word_bit == 63) ? ~word_t(0) : ((word_t(1) << (in_word_bit + 1)) - 1ULL));
            while (true) {
                if (topw) {
                    unsigned top_msb = msb_index(topw);
                    size_t prev_data_widx = (top_word_idx << 6) + top_msb;
                    // find msb in that data word
                    assert(prev_data_widx < data_words());
                    word_t dw = data_[prev_data_widx];
                    unsigned msb_dw = msb_index(dw);
                    return (prev_data_widx << 6) + msb_dw;
                }
                if (top_word_idx == 0) break;
                --top_word_idx;
                topw = top_[top_word_idx];
            }
            return std::nullopt;
        }

        // successor: smallest set bit >= pos. returns std::nullopt if none.
        std::optional<size_t> find_next(size_t pos) const {
            assert(pos <= (data_words() * WORD_BITS) - 1);
            size_t widx = pos >> 6;
            unsigned b = pos & 63;

            // mask bits >= b in same word
            word_t w = data_[widx] & ~((word_t(1) << b) - 1ULL);
            if (w) [[likely]] {
                unsigned lsb = lsb_index(w);
                return (widx << 6) + lsb;
            }

            // find next non-empty data-word by checking top level bits > widx
            size_t top_bit_idx = widx + 1;
            size_t top_word_idx = top_bit_idx >> 6;
            unsigned in_word_bit = top_bit_idx & 63;

            // mask top word bits >= in_word_bit
            if (top_word_idx < top_words()) {
                word_t topw = top_[top_word_idx] & (~((in_word_bit == 0) ? 0ULL : ((word_t(1) << in_word_bit) - 1ULL)));
                while (true) {
                    if (topw) {
                        unsigned top_lsb = lsb_index(topw);
                        size_t nxt_data_widx = (top_word_idx << 6) + top_lsb;
                        if (nxt_data_widx >= data_words()) return std::nullopt;
                        word_t dw = data_[nxt_data_widx];
                        unsigned lsb_dw = lsb_index(dw);
                        return (nxt_data_widx << 6) + lsb_dw;
                    }
                    ++top_word_idx;
                    if (top_word_idx >= top_words()) break;
                    topw = top_[top_word_idx];
                }
            }
            return std::nullopt;
        }

        // number of words holding data
        size_t data_words() const { return data_.size(); }

        // number of top words
        size_t top_words() const { return top_.size(); }

        void clear() {
            TwoLevelBitvector replace(data_words() * WORD_BITS);
            std::swap(*this, replace);
        }

        template<typename It>
        void bulk_load(It begin, It end) {
            while (begin != end) {
                insert(*begin);
                begin++;
            }
        }

    private:
        std::vector<word_t> data_; // data words storing actual bits
        std::vector<word_t> top_;  // top-level words: each bit corresponds to whether a data_ word is non-zero

        // ensure capacity for bit pos
        void ensure_capacity_for(size_t pos) {
            size_t need_words = (pos >> 6) + 1;
            if (need_words > data_words()) {
                data_.resize(need_words, 0);
                size_t needed_top_bits = need_words;
                size_t needed_top_words = (needed_top_bits + WORD_BITS - 1) / WORD_BITS;
                top_.resize(needed_top_words, 0);
            }
        }

        // set/clear top bit for given data word index
        void set_top_bit(size_t data_word_index) {
            size_t tidx = data_word_index >> 6;
            unsigned tbit = data_word_index & 63;
            top_[tidx] |= (word_t(1) << tbit);
        }

        void clear_top_bit(size_t data_word_index) {
            size_t tidx = data_word_index >> 6;
            unsigned tbit = data_word_index & 63;
            top_[tidx] &= ~(word_t(1) << tbit);
        }

        // find index (0..63) of least-significant 1-bit; undefined if x==0
        static unsigned lsb_index(word_t x) {
#if defined(__GNUG__) || defined(__clang__)
            return static_cast<unsigned>(__builtin_ctzll(x));
#else
            // fallback (slower)
        unsigned i = 0;
        while ((x & 1ULL) == 0) { x >>= 1; ++i; }
        return i;
#endif
        }

        // find index (0..63) of most-significant 1-bit; undefined if x==0
        static unsigned msb_index(word_t x) {
#if defined(__GNUG__) || defined(__clang__)
            return static_cast<unsigned>(63 - __builtin_clzll(x));
#else
            unsigned i = 63;
        while ((x >> i & 1ULL) == 0) --i;
        return i;
#endif
        }
    };


};
