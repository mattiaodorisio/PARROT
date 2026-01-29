#pragma once

#include <vector>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <cstdint>
#include <variant>
#include "bitvector.h"

#include "utils.h"
#include <experimental/simd>

namespace stdx = std::experimental;

namespace DeLI {
    enum class RhtOptimization {
        none,
        slot_index,
        gap_fill_predecessor,
        gap_fill_successor,
        gap_fill_both
    };


    template<bool dynamic, unsigned int value_bits, RhtOptimization opt, size_t simd_unrolled, size_t max_load_perc>
    class RHT {
        constexpr static bool use_simd = simd_unrolled > 0;
        static_assert(value_bits < 128);
        static_assert(max_load_perc < 100);
        static_assert(max_load_perc > 0);
        static_assert(RhtOptimization::gap_fill_successor != opt || !dynamic);
        static_assert(RhtOptimization::gap_fill_predecessor != opt || !dynamic);
        static_assert(RhtOptimization::gap_fill_both != opt || !dynamic);
        static_assert(RhtOptimization::slot_index != opt || !use_simd);
    private:
        using T = utils::uint_by_bits_t<value_bits + 1>; // we must be able to fit the extra empty_v value
        using sz_t = size_t;

        struct Stub {
            Stub(size_t) {
            }
        };

        std::conditional_t<opt == RhtOptimization::slot_index, TwoLevelBitvector, Stub> slot_bits;
        utils::AlignedVector<T> table;
        sz_t num_elements;
        sz_t slot_shift;
        sz_t begin_slot; // first slot outside of wrapping area

        constexpr static sz_t load_shifter = 16;
        constexpr static sz_t inv_max_load = sz_t(double((size_t(100) << load_shifter)) / double(max_load_perc));
        constexpr static sz_t shrink_threshold = 4;
        constexpr static T value_mask = utils::safe_shl(T(1), value_bits) - 1;

        // Sentinel values for empty slots
        constexpr static T empty_v = std::numeric_limits<T>::max();
        constexpr static T padding = std::numeric_limits<T>::max() - 1;

        using Tvec = stdx::native_simd<T>;
        constexpr static sz_t padding_length = simd_unrolled * Tvec::size();
        constexpr static sz_t simd_align_mask = ~(Tvec::size() - 1);

        constexpr static bool gap_fill_p =
                opt == RhtOptimization::gap_fill_predecessor || opt == RhtOptimization::gap_fill_both;
        constexpr static bool gap_fill_s =
                opt == RhtOptimization::gap_fill_successor || opt == RhtOptimization::gap_fill_both;
        constexpr static bool use_slot_index = opt == RhtOptimization::slot_index;

        std::conditional_t<dynamic, std::monostate, std::optional<T>> minV;
        std::conditional_t<dynamic, std::monostate, std::optional<T>> maxV;

        Tvec read_aligned(sz_t slot) const {
            return Tvec(&table[slot], stdx::vector_aligned);
        }

        bool isValue(T v) const {
            return v < padding;
        }

        void print_table() const {
            for (sz_t i = 0; i < table.size(); ++i) {
                if (isValue(table[i]))
                    std::cout << utils::to_string(table[i]) << " ";
                else
                    std::cout << ". ";
            }
            std::cout << std::endl;
        }

        sz_t scale(T key) const {
            return key == padding ? 0 : utils::safe_shr(key, slot_shift);
        }

        sz_t num_slot_target(sz_t new_num_elements) const {
            sz_t target = new_num_elements == 0 ? 0 : std::bit_ceil(
                    (inv_max_load * (new_num_elements + padding_length)) >> load_shifter);
            // ensure at most as many slots as possible values
            if constexpr (value_bits >= 64 - padding_length) {
                return target;
            } else {
                return std::min(target, std::bit_ceil(
                        (sz_t(1) << value_bits) + sz_t((use_simd && new_num_elements > 0) ? padding_length : 0)));
            }
        }

        template<bool alternating, bool succ>
        void fill_gaps(const std::vector<bool> &occupied_slots) {
            sz_t slot_mask = table.size() - 1;
            sz_t slot = succ ? begin_slot : (begin_slot - 1);
            T last_highest = std::numeric_limits<T>::max();
            while (true) {
                slot = (succ ? (slot - 1) : (slot + 1)) & slot_mask;
                if (occupied_slots[slot]) {
                    if (table[slot] != padding)
                        last_highest = table[slot];
                } else {
                    if (!alternating || slot % 2 == succ)
                        table[slot] = last_highest;
                }
                if (!succ && ((slot + 1) & slot_mask) == begin_slot) break;
                if (succ && slot == begin_slot) break;
            };
        }

        template<typename It>
        void insert_sorted(It begin, It end) {
            assert(std::is_sorted(begin, end));
            It begin_copy = begin;
            sz_t slot_mask = table.size() - 1;
            sz_t next_slot = 0; // points to the next free slot

            std::vector<bool> occupied_slots(table.size(), false);
            while (begin != end) {
                T v = *begin & value_mask;
                num_elements++;
                sz_t slot = scale(v);
                if (slot < next_slot) {
                    slot = (next_slot++) & slot_mask;
                } else {
                    next_slot = slot + 1;
                }
                table[slot] = v;
                occupied_slots[slot] = true;
                if constexpr (use_slot_index) {
                    slot_bits.insert(slot);
                }
                begin++;
            }
            if (next_slot > table.size()) {
                next_slot = next_slot & slot_mask;
            } else {
                next_slot = 0;
            }
            if constexpr (use_simd) {
                if (!table.empty()) {
                    for (sz_t i = 0; i < padding_length; ++i) {
                        sz_t slot = (next_slot++) & slot_mask;
                        occupied_slots[slot] = true;
                        table[slot] = padding;
                    }
                }
            }
            begin_slot = next_slot;
            // correct the elements at the beginning that are overwritten due to wrap around
            begin = begin_copy;
            while (begin != end) {
                T v = *begin & value_mask;
                if (scale(v) >= next_slot) {
                    break;
                }
                occupied_slots[next_slot] = true;
                if constexpr (use_slot_index) {
                    slot_bits.insert(next_slot);
                }
                table[next_slot++] = v;
                begin++;
            }
            if (!table.empty()) {
                if constexpr (gap_fill_s) {
                    fill_gaps<opt == RhtOptimization::gap_fill_both, true>(occupied_slots);
                }
                if constexpr (gap_fill_p) {
                    fill_gaps<opt == RhtOptimization::gap_fill_both, false>(occupied_slots);
                }
            }
            if constexpr (!dynamic) {
                if (!empty()) {
                    minV = *begin_copy;
                    maxV = *(end - 1);
                }
            }
        }

        template<typename It>
        RHT(It begin, It end, sz_t slots) : begin_slot(0), num_elements(0), table(slots, empty_v),
                                            slot_shift(value_bits - static_cast<sz_t>(std::countr_zero(slots))),
                                            slot_bits(slots) {
            insert_sorted(begin, end);
        }

        void ensure_scaling(sz_t new_num_elements) {
            sz_t slot_target = num_slot_target(new_num_elements);
            if (table.size() >= slot_target && table.size() < slot_target * shrink_threshold) {
                return;
            }
            RHT reseized = RHT(begin(), end(), slot_target);
            std::swap(*this, reseized);
        }

    public:

        RHT() : table(0), begin_slot(0), slot_shift(0), num_elements(0), slot_bits(0) {
        }

        template<typename It>
        void bulk_load(It b, It e, size_t keys) {
            assert(std::distance(b, e) == keys);
            size_t opt_size = num_slot_target(keys);
            RHT replacement = RHT(b, e, opt_size);
            std::swap(*this, replacement);
        }

        template<typename It>
        void bulk_load(It begin, It end) {
            bulk_load(begin, end, std::distance(begin, end));
        }

        bool insert(T key) requires(dynamic) {
            ensure_scaling(num_elements + 1);
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            sz_t start_probe = probe;

            while (table[probe] < key && isValue(table[probe])) {
                probe = (probe + 1) & slot_mask;
                if (probe == begin_slot) {
                    break;
                }
            }
            if (table[probe] == key) {
                // Key already exists
                return false;
            }
            while (table[probe] != empty_v) {
                // also shift padding
                std::swap(key, table[probe]);
                probe = (probe + 1) & slot_mask;
            }
            table[probe] = key;
            if constexpr (use_slot_index) {
                slot_bits.insert(probe);
            }
            ++num_elements;
            if (probe < start_probe) // wrapped around
                begin_slot++;
            return true;
        }

        bool insert(T key) requires(!dynamic) = delete;

        bool remove(T key) requires(dynamic) {
            if (table.empty()) {
                return false;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            sz_t probe_begin = probe;
            while (table[probe] < key && isValue(table[probe])) {
                probe = (probe + 1) & slot_mask;
                if (probe == begin_slot) {
                    break;
                }
            }
            if (table[probe] != key) {
                return false;
            }
            // Remove the key and shift elements to fill the gap
            while (true) {
                sz_t next_probe = (probe + 1) & slot_mask;
                if (table[next_probe] == empty_v || scale(table[next_probe]) == next_probe) {
                    table[probe] = empty_v;
                    if constexpr (use_slot_index) {
                        slot_bits.remove(probe);
                    }
                    break;
                }
                table[probe] = table[next_probe];
                probe = next_probe;
            }
            if (probe < probe_begin) {
                begin_slot--; // wrapped around
            }
            num_elements--;
            ensure_scaling(num_elements);
            return true;
        }

        bool remove(T key) requires(!dynamic) = delete;

        void clear() {
            RHT empty_rht;
            std::swap(*this, empty_rht);
        }

        bool empty() const {
            return num_elements == 0;
        }

        size_t size() const {
            return num_elements;
        }

        class simd_pred_succ_transform {
        public:
            static auto trans(auto a, auto b) {
                return a - b;
            }

            static auto inv_trans(auto a, auto b) {
                return a + b;
            }

            static bool stop(auto last, auto key) {
                return last == padding;
            }
        };

        class simd_pred : public simd_pred_succ_transform{
        public:
            static auto vert(auto a, auto b) {
                return stdx::max(a, b);
            }

            static auto horiz(auto a) {
                return stdx::hmax(a);
            }
        };

        class simd_succ : public simd_pred_succ_transform{
        public:
            static auto vert(auto a, auto b) {
                return stdx::min(a, b);
            }

            static auto horiz(auto a) {
                return stdx::hmin(a);
            }
        };

        class simd_contains {
        public:
            static auto trans(auto a, auto b) {
                return a == b;
            }

            static auto inv_trans(auto a, auto b) {
                return a;
            }

            static auto vert(auto a, auto b) {
                return a | b;
            }

            static auto horiz(auto a) {
                return stdx::any_of(a);
            }

            static bool stop(auto last, auto key) {
                return last > key;
            }
        };

        template<int dir, typename op>
        std::tuple<T, bool> simd_probe_iter(sz_t &probe, const T key, const T slot_mask) const requires(use_simd) {
            Tvec last;
            auto v1 = read_aligned(probe);
            auto combined = op::trans(v1, key);
            if constexpr (simd_unrolled > 1) {
                probe = (probe + dir * Tvec::size()) & slot_mask;
                auto v2 = read_aligned(probe);
                combined = op::vert(combined, op::trans(v2, key));
                if constexpr (simd_unrolled > 2) {
                    probe = (probe + dir * Tvec::size()) & slot_mask;
                    auto v3 = read_aligned(probe);
                    combined = op::vert(combined, op::trans(v3, key));
                    last = v3;
                    static_assert(simd_unrolled <= 3);
                } else {
                    last = v2;
                }
            } else {
                last = v1;
            }
            probe = (probe + dir * Tvec::size()) & slot_mask;
            bool in_padding = op::stop(last[dir > 0 ? (Tvec::size() - 1) : 0], key);
            T res = op::inv_trans(op::horiz(combined), key);
            return {res, in_padding};
        }

        template<int dir, typename op>
        std::tuple<T, bool>
        simd_probe_iter(sz_t &probe, const T key, const T slot_mask) const requires(!use_simd) = delete;

        /**
        * Find successor
        * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
        */
        std::optional<T> find_next(T key) const requires(!use_simd) {
            if (empty()) {
                return std::nullopt;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (!isValue(table[probe]) || table[probe] < key) {
                if constexpr (use_slot_index) {
                    if (!isValue(table[probe])) {
                        std::optional<T> res = slot_bits.find_next(probe);
                        return res.has_value() ? table[res.value()] : std::optional<T>{};
                    }
                }
                probe = (probe + 1) & slot_mask;
                if (probe == begin_slot)
                    return std::nullopt;
            }
            return table[probe];
        }

        std::optional<T> find_next(T key) const requires(use_simd) {
            if (empty()) {
                return std::nullopt;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key)) & simd_align_mask;
            while (true) {
                auto [res, in_padding] = simd_probe_iter<1, simd_succ>(probe, key, slot_mask);
                if (res >= key && isValue(res)) [[likely]] {
                    return res;
                }

                if (in_padding) [[unlikely]]
                    return std::nullopt;
            }
        }

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        std::optional<T> find_prev(T key) const requires(!use_simd) {
            if (empty()) {
                return std::nullopt;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));

            if (table[probe] >= key || !isValue(table[probe])) {
                if constexpr (use_slot_index) {
                    // skip towards left
                    std::optional<T> res;
                    if (probe <= begin_slot || !(res = slot_bits.find_prev(probe - 1)).has_value() ||
                        res.value() < begin_slot) {
                        return std::nullopt;
                    }
                    return table[res.value()];
                } else {
                    // probe towards left
                    do {
                        if (probe == begin_slot)
                            return std::nullopt;
                        probe = (probe - 1) & slot_mask;
                    } while (table[probe] >= key);
                    return table[probe];
                }
            } else {
                // cluster, probe towards right
                while (true) {
                    auto next = (probe + 1) & slot_mask;
                    if (table[next] >= key ||
                        !isValue(table[next]) ||
                        next == begin_slot) {
                        break;
                    }
                    probe = next;
                }
                return table[probe];
            }
        }

        std::optional<T> find_prev(T key) const requires(use_simd) {
            if (empty()) {
                return std::nullopt;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            bool probe_left = table[probe] >= key || !isValue(table[probe]);
            probe = probe & simd_align_mask;
            if (probe_left) {
                while (true) {
                    auto [res, in_padding] = simd_probe_iter<-1, simd_pred>(probe, key, slot_mask);
                    if (res < key && isValue(res)) [[likely]] {
                        return res;
                    }
                    if (in_padding) [[unlikely]]
                        return std::nullopt;
                }
            } else {
                std::optional<T> highest_pred = std::nullopt;
                while (true) {
                    auto [res, in_padding] = simd_probe_iter<1, simd_pred>(probe, key, slot_mask);
                    if (res < key && isValue(res)) [[likely]] {
                        highest_pred = res;
                    }
                    if (in_padding) [[unlikely]]
                        return highest_pred;
                }
            }
        }

        bool contains(T key) const requires(!use_simd) {
            if (empty()) {
                return false;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (table[probe] < key && isValue(table[probe])) {
                probe = (probe + 1) & slot_mask;
                if (probe == begin_slot) {
                    break;
                }
            }
            return (table[probe] == key);
        }

        bool contains(T key) const requires(use_simd)  {
            if (empty()) {
                return false;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key)) & simd_align_mask;
            while (true) {
                auto [res, ends] = simd_probe_iter<1, simd_contains>(probe, key, slot_mask);
                if (res || ends) [[likely]] {
                    return res;
                }
            }
        }

        std::optional<T> min() const {
            if constexpr (!dynamic) {
                return minV;
            }
            return find_next(0);
        }

        std::optional<T> max() const requires(!dynamic) {
            return maxV;
        }

        std::optional<T> max() const requires(!use_simd && dynamic) {
            if (empty()) {
                return std::nullopt;
            }
            sz_t slot_mask = table.size() - 1;
            sz_t probe = (begin_slot - 1) & slot_mask;
            while (!isValue(table[probe])) {
                probe = (probe - 1) & slot_mask;
            }
            return table[probe];
        }

        std::optional<T> max() const requires(use_simd && dynamic) {
            if (empty()) {
                return std::nullopt;
            }
            sz_t slot_mask = table.size() - 1;
            sz_t probe = ((begin_slot - padding_length) & slot_mask) & simd_align_mask;
            while (true) {
                auto [res, in_padding] = simd_probe_iter<-1, simd_pred>(probe, value_mask + 1, slot_mask);
                if (isValue(res)) [[likely]] {
                    return res;
                }
            }
        }

        bool operator==(const RHT &other) const {
            if (table.size() != other.table.size())
                return false;

            for (sz_t i = 0; i < table.size(); ++i) {
                if (table[i] != other.table[i]) return false;
            }
            return true;
        }


        class const_iterator {
        private:
            friend class RHT;

            sz_t index;
            const RHT &rht;

            const_iterator(sz_t index, const RHT &rht_ref) : index(index), rht(rht_ref) {}

            const_iterator(bool begin, const RHT &rht_ref) : rht(rht_ref) {
                if (!begin || rht.empty()) {
                    index = sz_t(-1);
                } else {
                    index = rht.begin_slot;
                    while (!rht.isValue(rht.table[index])) {
                        index++;
                        if (index == rht.table.size()) {
                            index = sz_t(-1);
                            break;
                        }
                    }
                }
            }

        public:
            const_iterator(const const_iterator &other) : index(other.index), rht(other.rht) {}

            const_iterator(const_iterator &&other) noexcept: index(other.index), rht(other.rht) {}

            const_iterator &operator=(const const_iterator &other) {
                if (this == &other) return *this;
                this->~const_iterator();
                new(this) const_iterator(other);
                return *this;
            }

            const_iterator &operator=(const_iterator &&other) noexcept {
                if (this == &other) return *this;
                this->~const_iterator();
                new(this) const_iterator(std::move(other));
                return *this;
            }

            const_iterator &operator++() {
                assert(index != sz_t(-1));
                T prev = operator*();
                while (true) {
                    index = (index + 1) & (rht.table.size() - 1);
                    if (index == rht.begin_slot) {
                        index = sz_t(-1);
                        return *this;
                    }
                    if (rht.isValue(rht.table[index]) && rht.table[index] > prev) {
                        return *this;
                    }
                };
            }

            const_iterator operator++(int) {
                const_iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            T operator*() const {
                return rht.table[index];
            }

            const T *operator->() const {
                return &rht.table[index];
            }

            bool operator==(const const_iterator &other) const { return index == other.index; }

            bool operator!=(const const_iterator &other) const { return index != other.index; }
        };

        const_iterator begin() const {
            return const_iterator(true, *this);
        }

        const_iterator end() const {
            return const_iterator(false, *this);
        }

        const_iterator cbegin() const { return begin(); }

        const_iterator cend() const { return end(); }

    };
}
