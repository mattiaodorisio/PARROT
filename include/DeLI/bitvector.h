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
        explicit TwoLevelBitvector()
            : min_val_(0), max_val_(0), is_empty_(true) {
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

        // set bit at pos (0-based), with min-based coordinate transformation
        void insert(size_t pos) {
            if (is_empty_) {
                min_val_ = pos;
                max_val_ = pos;
                is_empty_ = false;
                // Allocate initial space
                resize(64);
                insert_internal(0);
                return;
            }

            if (pos < min_val_) {
                // Shift all existing bits up to make room for new minimum
                size_t delta = min_val_ - pos;
                shift_up(delta);
                min_val_ = pos;
            }
            if (pos > max_val_) {
                max_val_ = pos;
            }

            // Transform position and insert
            size_t transformed_pos = pos - min_val_;
            insert_internal(transformed_pos);
        }

        // Internal insert without transformation
        void insert_internal(size_t pos) {
            // Ensure we have enough capacity
            size_t needed_bits = pos + 1;
            if (needed_bits > data_words() * WORD_BITS) {
                resize(needed_bits);
            }

            assert(pos < data_words() * WORD_BITS);
            size_t widx = pos >> 6;
            unsigned b = pos & 63;
            word_t old = data_[widx];
            data_[widx] |= (word_t(1) << b);
            if (old == 0) // transition 0 -> non-zero: set top bit
                set_top_bit(widx);
        }

        // clear bit at pos, with min-based coordinate transformation
        void remove(size_t pos) {
            if (is_empty_) return;
            if (pos < min_val_ || pos > max_val_) return;

            // Transform position
            size_t transformed_pos = pos - min_val_;
            remove_internal(transformed_pos);

            // If we removed min or max, find the new ones
            if (pos == min_val_ || pos == max_val_) {
                update_min_max();
            }
        }

        // Internal remove without transformation
        void remove_internal(size_t pos) {
            if (pos >= data_words() * WORD_BITS) return;

            size_t widx = pos >> 6;
            unsigned b = pos & 63;
            data_[widx] &= ~(word_t(1) << b);
            if (data_[widx] == 0) // transition to zero: clear top bit
                clear_top_bit(widx);
        }

        // Update min and max after a deletion, using find_prev and find_next
        void update_min_max() {
            // Find the minimum set bit
            auto min_opt = find_next_internal(0);
            if (!min_opt) {
                // Data structure is now empty
                is_empty_ = true;
                min_val_ = 0;
                max_val_ = 0;
                return;
            }

            // Find the maximum set bit
            auto max_opt = find_prev_internal(data_words() * WORD_BITS - 1);

            size_t base = min_val_;
            min_val_ = *min_opt + base;
            max_val_ = *max_opt + base;
        }

        // test bit at pos, with min-based coordinate transformation
        bool contains(size_t pos) const {
            if (is_empty_) return false;
            if (pos < min_val_ || pos > max_val_) return false;
            size_t transformed_pos = pos - min_val_;
            return contains_internal(transformed_pos);
        }

        // Internal contains without transformation
        bool contains_internal(size_t pos) const {
            if (pos >= data_words() * WORD_BITS) return false;
            size_t widx = pos >> 6;
            unsigned b = pos & 63;
            return (data_[widx] >> b) & 1ULL;
        }

        // predecessor: greatest set bit <= pos (with min-based coordinate transformation). returns std::nullopt if none.
        std::optional<size_t> find_prev(size_t pos) const {
            if (is_empty_) return std::nullopt;

            // If pos < min_val, there's no predecessor
            if (pos < min_val_) return std::nullopt;

            // If pos >= max_val, the predecessor is at most max_val, so search from max_val
            size_t search_pos = (pos >= max_val_) ? max_val_ : pos;

            size_t transformed_pos = search_pos - min_val_;
            auto result = find_prev_internal(transformed_pos);
            if (result) {
                return *result + min_val_;
            }
            return std::nullopt;
        }

        // successor: smallest set bit >= pos (with min-based coordinate transformation). returns std::nullopt if none.
        std::optional<size_t> find_next(size_t pos) const {
            if (is_empty_) return std::nullopt;

            // If pos > max_val, there's no successor
            if (pos > max_val_) return std::nullopt;

            // If pos <= min_val, the successor is at least min_val, so search from min_val
            size_t search_pos = (pos <= min_val_) ? min_val_ : pos;

            size_t transformed_pos = search_pos - min_val_;
            auto result = find_next_internal(transformed_pos);
            if (result) {
                return *result + min_val_;
            }
            return std::nullopt;
        }

        // number of words holding data
        size_t data_words() const { return data_.size(); }

        // number of top words
        size_t top_words() const { return top_.size(); }

        void clear() {
            TwoLevelBitvector replace;
            std::swap(*this, replace);
        }

        template<typename It>
        void bulk_load(It begin, It end) {
            while (begin != end) {
                insert(*begin);
                begin++;
            }
        }

        [[nodiscard]] size_t min() const {
            return min_val_;
        }

        [[nodiscard]] size_t max() const {
            return max_val_ + 1;
        }

    private:
        std::vector<word_t> data_; // data words storing actual bits
        std::vector<word_t> top_;  // top-level words: each bit corresponds to whether a data_ word is non-zero
        size_t min_val_, max_val_; // track min and max values inserted
        bool is_empty_;            // whether the data structure is empty

        // Shift all stored bits up by delta positions (used when min_val_ decreases).
        void shift_up(size_t delta) {
            std::vector<size_t> positions;
            for (size_t i = 0; i < data_words(); ++i) {
                word_t w = data_[i];
                while (w) {
                    unsigned lsb = lsb_index(w);
                    positions.push_back((i << 6) + lsb);
                    w &= w - 1; // clear the least significant bit
                }
            }

            // Clear the data structure
            data_.clear();
            top_.clear();
            resize(64); // Start with minimal size

            // Re-insert all positions with the new transformation
            for (size_t pos : positions) {
                insert_internal(pos + delta);
            }
        }

        // Find next set bit in the transformed space (internal, without transformation)
        std::optional<size_t> find_next_internal(size_t pos) const {
            if (pos >= data_words() * WORD_BITS) return std::nullopt;
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

        // Find previous set bit in the transformed space (internal, without offset transformation)
        std::optional<size_t> find_prev_internal(size_t pos) const {
            if (pos >= data_words() * WORD_BITS) return std::nullopt;
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
