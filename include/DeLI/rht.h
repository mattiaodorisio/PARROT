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

namespace DeLI {
    enum class RhtOptimization {
        none,
        slot_index,
        gap_fill_predecessor,
        gap_fill_successor,
        gap_fill_both
    };


    template<bool dynamic, unsigned int value_bits, RhtOptimization opt>
    class RHT {
        static_assert(value_bits < 128);
        static_assert(RhtOptimization::gap_fill_successor != opt || !dynamic);
        static_assert(RhtOptimization::gap_fill_predecessor != opt || !dynamic);
        static_assert(RhtOptimization::gap_fill_both != opt || !dynamic);
    private:
        using T = utils::uint_by_bits_t<value_bits + 1>; // we must be able to fit the extra empty_v value
        using sz_t = size_t;
        struct Stub {
            Stub(size_t) {
            }
        };
        std::conditional_t<opt == RhtOptimization::slot_index, TwoLevelBitvector, Stub> slot_bits;
        std::vector<T> table;
        sz_t num_elements;
        sz_t slot_shift;
        sz_t begin_slot; // first slot outside of wrapping area

        constexpr static sz_t inv_max_load = 2;
        constexpr static sz_t shrink_threshold = 4;
        constexpr static T value_mask = utils::safe_shl(T(1), value_bits) - 1;

        // Sentinel values for empty slots
        constexpr static T empty_v = std::numeric_limits<T>::max();

        constexpr static bool gap_fill_p =
                opt == RhtOptimization::gap_fill_predecessor || opt == RhtOptimization::gap_fill_both;
        constexpr static bool gap_fill_s =
                opt == RhtOptimization::gap_fill_successor || opt == RhtOptimization::gap_fill_both;
        constexpr static bool use_slot_index = opt == RhtOptimization::slot_index;

        std::conditional_t<dynamic, std::monostate, std::optional<T>> minV;
        std::conditional_t<dynamic, std::monostate, std::optional<T>> maxV;

        void print_table() const {
            for (sz_t i = 0; i < table.size(); ++i) {
                if (table[i] == empty_v)
                    std::cout << ". ";
                else
                    std::cout << table[i] << " ";
            }
            std::cout << std::endl;
        }

        sz_t scale(T key) const {
            return utils::safe_shr(key, slot_shift);
        }

        sz_t num_slot_target(sz_t new_num_elements) const {
            //trick to make 0 map to 0
            sz_t target = std::bit_ceil(size_t(2 * inv_max_load * new_num_elements)) >> 1;
            // ensure at most as many slots as possible values
            if constexpr (value_bits >= 64) {
                return target;
            } else {
                return std::min(target, sz_t(1) << value_bits);
            }
        }

        template<bool alternating, bool succ>
        void fill_gaps() {
            sz_t slot_mask = table.size() - 1;
            sz_t slot = succ ? begin_slot : (begin_slot - 1);
            T last_highest = std::numeric_limits<T>::max();
            do {
                slot = (succ ? (slot - 1) : (slot + 1)) & slot_mask;
                if (table[slot] == empty_v) {
                    if (!alternating || slot % 2 == succ)
                        table[slot] = last_highest | (T(1) << value_bits);
                } else {
                    if (table[slot] >> value_bits == 0)
                        last_highest = table[slot];
                }
            } while (slot != begin_slot);
        }

        template<typename It>
        void insert_sorted(It begin, It end) {
            assert(std::is_sorted(begin, end));
            It begin_copy = begin;
            sz_t slot_mask = table.size() - 1;
            sz_t next_slot = 0; // points to the next free slot
            while (begin != end) {
                T v = *begin & value_mask;
                if (v != empty_v) {
                    num_elements++;
                    sz_t slot = scale(v);
                    if (slot < next_slot) {
                        slot = (next_slot++) & slot_mask;
                    } else {
                        next_slot = slot + 1;
                    }
                    table[slot] = v;
                    if constexpr (use_slot_index) {
                        slot_bits.insert(slot);
                    }
                }
                begin++;
            }
            begin = begin_copy;
            if (next_slot > table.size()) {
                // we had a wrap around
                next_slot = next_slot & slot_mask;
                begin_slot = next_slot;
                // correct the elements at the beginning that are overwritten due to wrap around
                while (begin != end) {
                    T v = *begin & value_mask;
                    if (v != empty_v) {
                        if (scale(v) >= next_slot) {
                            break;
                        }
                        if constexpr (use_slot_index) {
                            slot_bits.insert(next_slot);
                        }
                        table[next_slot++] = v;
                    }
                    begin++;
                }
            }
            if (!table.empty()) {
                if constexpr (gap_fill_s) {
                    fill_gaps<opt == RhtOptimization::gap_fill_both, true>();
                }
                if constexpr (gap_fill_p) {
                    fill_gaps<opt == RhtOptimization::gap_fill_both, false>();
                }
                if constexpr (gap_fill_s || gap_fill_p) {
                    for (int i = 0; i < table.size(); ++i) {
                        if (table[i] != empty_v) {
                            table[i] &= value_mask;
                        }
                    }
                }
            }
            if constexpr (!dynamic) {
                if(!empty()) {
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
            if (table.size() >= slot_target && table.size() <= slot_target * shrink_threshold) {
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
            RHT replacement = RHT(b, e, num_slot_target(keys));
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

            while (table[probe] < key && table[probe] != empty_v) {
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
            while (table[probe] < key && table[probe] != empty_v) {
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

        bool contains(T key) const {
            if (empty()) {
                return false;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (table[probe] < key && table[probe] != empty_v) {
                probe = (probe + 1) & slot_mask;
                if (probe == begin_slot) {
                    break;
                }
            }
            return (table[probe] == key);
        }

        void clear() {
            RHT empty_rht;
            std::swap(*this, empty_rht);
        }

        bool empty() const {
            return num_elements == 0;
        }

        inline std::optional<T> min() const {
            if constexpr (!dynamic) {
                return minV;
            }
            if (empty()) {
                return std::nullopt;
            }
            sz_t probe = begin_slot;
            while (table[probe] == empty_v) {
                ++probe;
            }
            return table[probe];
        }

        inline std::optional<T> max() const {
            if constexpr (!dynamic) {
                return maxV;
            }
            if (empty()) {
                return std::nullopt;
            }
            sz_t slot_mask = table.size() - 1;
            sz_t probe = (begin_slot - 1) & slot_mask;
            while (table[probe] == empty_v) {
                --probe;
            }
            return table[probe];
        }

        size_t d_size() {
            size_t cnt = 0;
            for (int i = 0; i < table.size(); ++i) {
                if (table[i] != empty_v)
                    cnt++;
            }
            return cnt;
        }

        size_t size() const {
            return num_elements;
        }

        /**
        * Find successor
        * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
        */
        std::optional<T> find_next(T key) const {
            if (empty()) {
                return std::nullopt;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            bool inGap;
            while ((inGap = table[probe] == empty_v) || table[probe] < key) {
                if constexpr (use_slot_index) {
                    if (inGap) {
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

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        std::optional<T> find_prev(T key) const {
            if (empty()) {
                return std::nullopt;
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));

            if (table[probe] >= key || table[probe] == empty_v) {
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
                        table[next] == empty_v ||
                        next == begin_slot) {
                        break;
                    }
                    probe = next;
                }
                return table[probe];
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
            sz_t index;
            const RHT &rht;
        public:
            const_iterator(bool begin, const RHT &rht_ref) : rht(rht_ref) {
                if (!begin || rht.empty()) {
                    index = sz_t(-1);
                } else {
                    index = rht.begin_slot;
                    while (rht.table[index] == empty_v) {
                        index++;
                        if (index == rht.table.size()) {
                            index = sz_t(-1);
                            break;
                        }
                    }
                }
            }

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
                while (true) {
                    index = (index + 1) & (rht.table.size() - 1);
                    if (index == rht.begin_slot) {
                        index = sz_t(-1);
                        return *this;
                    }
                    if (rht.table[index] != empty_v) {
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
