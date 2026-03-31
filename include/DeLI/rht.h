#pragma once

#include <vector>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <cstdint>
#include <variant>
#include <type_traits>
#include <array>
#include <utility>
#include "bitvector.h"

#include "utils.h"
#include <experimental/simd>

namespace stdx = std::experimental;

namespace DeLI {
    struct NoPayload {
    };

    enum class RhtOptimization {
        none,
        slot_index,
        gap_fill_predecessor,
        gap_fill_successor,
        gap_fill_both
    };


    template<bool dynamic, unsigned int value_bits, RhtOptimization opt, size_t simd_unrolled, size_t max_load_perc,
             typename PayloadT = NoPayload>
    class RHT {
        constexpr static bool use_simd = simd_unrolled > 0;
        static_assert(value_bits < 128);
        static_assert(value_bits > 0);
        static_assert(max_load_perc < 100);
        static_assert(max_load_perc > 0);
        static_assert(RhtOptimization::gap_fill_successor != opt || !dynamic);
        static_assert(RhtOptimization::gap_fill_predecessor != opt || !dynamic);
        static_assert(RhtOptimization::gap_fill_both != opt || !dynamic);
        static_assert(RhtOptimization::slot_index != opt || !use_simd);
        static constexpr bool has_payload = !std::is_same_v<PayloadT, NoPayload>;
    public:
        using T = utils::uint_by_bits_t<value_bits + 1>; // we must be able to fit the extra empty_v value
        using Tvec = stdx::native_simd<T>;
        using payload_t = PayloadT;
        class const_iterator;
        constexpr static size_t simd_width = utils::simd_bit_width<Tvec>();
        constexpr static bool has_payloads = has_payload;

    private:
        using sz_t = size_t;

        template<typename It>
        static constexpr bool iter_has_key = requires(const It &it) {
            it.key();
        };

        template<typename It>
        static constexpr bool iter_has_payload = requires(const It &it) {
            it.payload();
        };

        template<typename It>
        static constexpr bool iter_has_first = requires(const It &it) {
            it.first;
        };

        template<typename It>
        static constexpr bool iter_has_second = requires(const It &it) {
            it.second;
        };

        template<typename It>
        using iter_entry_t = std::remove_cvref_t<decltype(*std::declval<const It &>())>;

        template<typename It>
        static constexpr bool iter_entry_has_key = requires(const iter_entry_t<It> &e) {
            e.key();
        };

        template<typename It>
        static constexpr bool iter_entry_has_payload = requires(const iter_entry_t<It> &e) {
            e.payload();
        };

        template<typename It>
        static constexpr bool iter_entry_has_first = requires(const iter_entry_t<It> &e) {
            e.first;
        };

        template<typename It>
        static constexpr bool iter_entry_has_second = requires(const iter_entry_t<It> &e) {
            e.second;
        };

        template<typename It>
        static T iter_key(const It &it) {
            if constexpr (iter_has_key<It>) {
                return it.key();
            } else if constexpr (iter_entry_has_key<It>) {
                return (*it).key();
            } else if constexpr (iter_has_first<It>) {
                return static_cast<T>(it.first);
            } else if constexpr (iter_entry_has_first<It>) {
                return static_cast<T>((*it).first);
            } else {
                return static_cast<T>(*it);
            }
        }

        template<typename It>
        static payload_t iter_payload(const It &it) requires(has_payload) {
            if constexpr (iter_has_payload<It>) {
                return it.payload();
            } else if constexpr (iter_entry_has_payload<It>) {
                return (*it).payload();
            } else if constexpr (iter_has_second<It>) {
                return it.second;
            } else if constexpr (iter_entry_has_second<It>) {
                return (*it).second;
            } else {
                return payload_t{};
            }
        }

        // Static assertion: if has_payload, iterator must provide either payload() or .second
        template<typename It>
        static constexpr void validate_payload_iter() requires(has_payload) {
            static_assert(iter_has_payload<It> || iter_has_second<It> || iter_entry_has_payload<It> ||
                          iter_entry_has_second<It>,
                "When has_payload is true, iterator must provide either .payload() method or .second member (pair-like)");
        }

        struct Stub {
            Stub(size_t) {
            }
        };

        using PayloadStorage_t = std::conditional_t<has_payload, utils::AlignedVector<payload_t>, Stub>;
        std::conditional_t<opt == RhtOptimization::slot_index, TwoLevelBitvector, Stub> slot_bits;
        utils::AlignedVector<T> table;
        PayloadStorage_t payload_table;
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

        constexpr static sz_t padding_length = simd_unrolled * Tvec::size();
        constexpr static sz_t simd_align_mask = ~(Tvec::size() - 1);

        constexpr static bool gap_fill_p =
                opt == RhtOptimization::gap_fill_predecessor || opt == RhtOptimization::gap_fill_both;
        constexpr static bool gap_fill_s =
                opt == RhtOptimization::gap_fill_successor || opt == RhtOptimization::gap_fill_both;
        constexpr static bool use_slot_index = opt == RhtOptimization::slot_index;

        using static_minmax_cache_t = std::conditional_t<has_payload, std::optional<sz_t>, std::optional<T>>;
        std::conditional_t<dynamic, std::monostate, static_minmax_cache_t> minV;
        std::conditional_t<dynamic, std::monostate, static_minmax_cache_t> maxV;

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
            [[maybe_unused]] sz_t last_payload_slot = 0;
            while (true) {
                slot = (succ ? (slot - 1) : (slot + 1)) & slot_mask;
                if (occupied_slots[slot]) {
                    if (table[slot] != padding) {
                        last_highest = table[slot];
                        if constexpr (has_payload) {
                            last_payload_slot = slot;
                        }
                    }
                } else {
                    if (!alternating || slot % 2 == succ) {
                        table[slot] = last_highest;
                        if constexpr (has_payload) {
                            payload_table[slot] = payload_table[last_payload_slot];
                        }
                    }
                }
                if (!succ && ((slot + 1) & slot_mask) == begin_slot) break;
                if (succ && slot == begin_slot) break;
            };
        }

        void clear_payload_slot(sz_t slot) {
            if constexpr (has_payload) {
                payload_table[slot] = payload_t{};
            }
        }

        template<typename It>
        void insert_sorted(It begin, It end) {
            assert(std::is_sorted(begin, end));
            It begin_copy = begin;
            sz_t slot_mask = table.size() - 1;
            sz_t next_slot = 0; // points to the next free slot
            std::conditional_t<has_payload, std::optional<sz_t>, std::optional<T>> first_seen = std::nullopt;
            std::conditional_t<has_payload, std::optional<sz_t>, std::optional<T>> last_seen = std::nullopt;
            
            std::vector<bool> occupied_slots(table.size(), false);
            while (begin != end) {
                T v = iter_key(begin) & value_mask;
                if constexpr (!has_payload) {
                    if (!first_seen) {
                        first_seen = v;
                    }
                    last_seen = v;
                }
                num_elements++;
                sz_t slot = scale(v);
                if (slot < next_slot) {
                    slot = (next_slot++) & slot_mask;
                } else {
                    next_slot = slot + 1;
                }
                table[slot] = v;
                if constexpr (has_payload) {
                    payload_table[slot] = iter_payload(begin);
                    if (!first_seen) {
                        first_seen = slot;
                    }
                    last_seen = slot;
                }
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
                        clear_payload_slot(slot);
                    }
                }
            }
            begin_slot = next_slot;
            // correct the elements at the beginning that are overwritten due to wrap around
            begin = begin_copy;
            [[maybe_unused]] bool first_wrap = true;
            while (begin != end) {
                T v = iter_key(begin) & value_mask;
                if (scale(v) >= next_slot) {
                    break;
                }
                occupied_slots[next_slot] = true;
                if constexpr (use_slot_index) {
                    slot_bits.insert(next_slot);
                }
                table[next_slot] = v;
                if constexpr (has_payload) {
                    payload_table[next_slot] = iter_payload(begin);
                    if (first_wrap) { // wrap: the min is now at next_slot
                        first_seen = next_slot;
                        first_wrap = false;
                    }
                }
                ++next_slot;
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
                    minV = first_seen;
                    maxV = last_seen;
                }
            }
        }



        template<typename It>
        RHT(It begin, It end, sz_t slots) : begin_slot(0), num_elements(0), table(slots, empty_v),
                                            payload_table(slots),
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

        RHT() : table(0), payload_table(0), begin_slot(0), slot_shift(0), num_elements(0), slot_bits(0) {
        }

        template<typename It>
        void bulk_load(It b, It e, size_t keys) {
            if constexpr (has_payload) {
                validate_payload_iter<It>();
            }
            assert(std::distance(b, e) == keys);
            size_t opt_size = num_slot_target(keys);
            RHT replacement = RHT(b, e, opt_size);
            std::swap(*this, replacement);
        }

        template<typename It>
        void bulk_load(It begin, It end) {
            bulk_load(begin, end, std::distance(begin, end));
        }

        bool insert(T key, const payload_t &payload = payload_t{}) requires(dynamic) {
            ensure_scaling(num_elements + 1);
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            sz_t start_probe = probe;
            payload_t payload_tmp = payload;

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
                if constexpr (has_payload) {
                    std::swap(payload_tmp, payload_table[probe]);
                }
                probe = (probe + 1) & slot_mask;
            }
            table[probe] = key;
            if constexpr (has_payload) {
                payload_table[probe] = payload_tmp;
            }
            if constexpr (use_slot_index) {
                slot_bits.insert(probe);
            }
            ++num_elements;
            if (probe < start_probe) // wrapped around
                begin_slot++;
            return true;
        }

        bool insert(T key, const payload_t &payload = payload_t{}) requires(!dynamic) = delete;

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
                    clear_payload_slot(probe);
                    if constexpr (use_slot_index) {
                        slot_bits.remove(probe);
                    }
                    break;
                }
                table[probe] = table[next_probe];
                if constexpr (has_payload) {
                    payload_table[probe] = payload_table[next_probe];
                }
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

        class simd_pred : public simd_pred_succ_transform {
        public:
            static auto vert(auto a, auto b) {
                return stdx::max(a, b);
            }

            static auto better(auto a, auto b) {
                return a > b;
            }

            static auto horiz(auto a) {
                return stdx::hmax(a);
            }
        };

        class simd_succ : public simd_pred_succ_transform {
        public:
            static auto vert(auto a, auto b) {
                return stdx::min(a, b);
            }

            static auto better(auto a, auto b) {
                return (a < b) | (a >= padding);
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


        // returns (combined, last)
        template<int dir, typename op, int i>
        auto simd_probe_unrolled(sz_t &probe, const T key, const sz_t slot_mask) const requires(use_simd) {
            auto v = read_aligned(probe);
            probe = (probe + dir * Tvec::size()) & slot_mask;
            auto vt = op::trans(v, key);
            if constexpr (i > 1) {
                auto [comb, last] = simd_probe_unrolled<dir, op, i - 1>(probe, key, slot_mask);
                return std::make_pair(op::vert(vt, comb), last);
            } else {
                return std::make_pair(vt, v);
            }
        }

        template<int dir, typename op, int i>
        auto simd_probe_unrolled(sz_t &probe, const T key, const sz_t slot_mask) const requires(!use_simd) = delete;

        template<int dir, typename op>
        std::tuple<T, bool> simd_probe_iter(sz_t &probe, const T key, const sz_t slot_mask) const requires(use_simd) {
            auto [comb, last] = simd_probe_unrolled<dir, op, simd_unrolled>(probe, key, slot_mask);
            bool is_stop = op::stop(last[dir > 0 ? (Tvec::size() - 1) : 0], key);
            T res = op::inv_trans(op::horiz(comb), key);
            return {res, is_stop};
        }

        template<int dir, typename op>
        std::tuple<T, bool>
        simd_probe_iter(sz_t &probe, const T key, const sz_t slot_mask) const requires(!use_simd) = delete;

        // returns (combined, last)
        template<int dir, typename op, int i>
        auto simd_probe_rank_unrolled(sz_t &probe, const T key, const sz_t slot_mask) const requires(use_simd) {
            // TODO: this doesn't work work for dir < 1
            auto v = read_aligned(probe);
            probe = (probe + dir * Tvec::size()) & slot_mask;
            auto keep = !op::better(v, key);
            auto first = stdx::any_of(keep) ? stdx::find_first_set(keep) : -1;
            sz_t vt = (first >= 0) ? static_cast<sz_t>(first) : Tvec::size();
            if constexpr (i > 1) {
                auto [comb, last] = simd_probe_rank_unrolled<dir, op, i - 1>(probe, key, slot_mask);
                return std::make_pair(vt < Tvec::size() ? vt : Tvec::size() + comb, last);
            } else {
                return std::make_pair((first >= 0) ? static_cast<sz_t>(first) : Tvec::size(), v);
            }
        }

        template<int dir, typename op, int i>
        auto simd_probe_rank_unrolled(sz_t &probe, const T key, const sz_t slot_mask) const requires(!use_simd) = delete;

        template<int dir, typename op>
        std::tuple<sz_t, bool> simd_probe_rank_iter(sz_t &probe, const T key, const sz_t slot_mask) const requires(use_simd) {
            auto [comb, last] = simd_probe_rank_unrolled<dir, op, simd_unrolled>(probe, key, slot_mask);
            bool is_stop = op::stop(last[dir > 0 ? (Tvec::size() - 1) : 0], key);
            return {comb, is_stop};
        }

        template<int dir, typename op>
        std::tuple<sz_t, bool>
        simd_probe_rank_iter(sz_t &probe, const T key, const sz_t slot_mask) const requires(!use_simd) = delete;
        

        /**
        * Find successor
        * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
        */
        const_iterator find_next_iter(T key) const requires(!use_simd) {
            if (empty()) {
                return end();
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (!isValue(table[probe]) || table[probe] < key) {
                if constexpr (use_slot_index) {
                    if (!isValue(table[probe])) {
                        std::optional<T> res = slot_bits.find_next(probe);
                        return res.has_value() ? const_iterator(static_cast<sz_t>(res.value()), *this) : end();
                    }
                }
                probe = (probe + 1) & slot_mask;
                if (probe == begin_slot)
                    return end();
            }
            return const_iterator(probe, *this);
        }

        const_iterator find_next_iter(T key) const requires(use_simd) {
            if (empty()) {
                return end();
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key)) & simd_align_mask;
            while (true) {
                const sz_t begin_slot = probe;
                auto [res, in_padding] = simd_probe_rank_iter<1, simd_succ>(probe, key, slot_mask);
                const sz_t candidate = (begin_slot + static_cast<sz_t>(res)) & slot_mask;

                constexpr sz_t no_hit = simd_unrolled * Tvec::size();                
                if (res != no_hit && table[candidate] >= key && isValue(table[candidate])) [[likely]] {
                    return const_iterator(candidate, *this);
                }

                if (in_padding) [[unlikely]]
                    return end();
            }
        }

        /**
        * Return iterator to minimum element in hash table
        */
        const_iterator min_iter() const {
            if constexpr (!dynamic) {
                if constexpr (has_payload) {
                    return minV ? const_iterator(minV.value(), *this) : end();
                } else {
                    return minV ? find_next_iter(minV.value()) : end();
                }
            }
            return find_next_iter(0);
        }

        /**
        * Return iterator to maximum element in hash table
        */
        const_iterator max_iter() const requires(!dynamic) {
            if constexpr (has_payload) {
                return maxV ? const_iterator(maxV.value(), *this) : end();
            } else {
                return maxV ? find_next_iter(maxV.value()) : end();
            }
        }

        const_iterator max_iter() const requires(!use_simd && dynamic) {
            if (empty()) {
                return end();
            }
            sz_t slot_mask = table.size() - 1;
            sz_t probe = (begin_slot - 1) & slot_mask;
            while (!isValue(table[probe])) {
                probe = (probe - 1) & slot_mask;
            }
            return const_iterator(probe, *this);
        }

        const_iterator max_iter() const requires(use_simd && dynamic) {
            if (empty()) {
                return end();
            }
            sz_t slot_mask = table.size() - 1;
            sz_t probe = ((begin_slot - padding_length) & slot_mask) & simd_align_mask;
            T highest_value = static_cast<T>(0);
            sz_t highest_probe = slot_mask + 1;  // invalid
            while (true) {
                auto [res, in_padding] = simd_probe_iter<-1, simd_pred>(probe, value_mask + 1, slot_mask);
                if (isValue(res)) [[likely]] {
                    highest_value = res;
                    highest_probe = probe - in_padding;
                    break;
                }
            }
            if (highest_probe > slot_mask) {
                return end();
            }
            return const_iterator(highest_probe, *this);
        }

        /**
        * Find predecesor iterator
        * returns iterator to the first element STRICTLY LESS than the given key
        */
        const_iterator find_prev_iter(T key) const requires(!use_simd) {
            if (empty()) {
                return end();
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));

            if (table[probe] >= key || !isValue(table[probe])) {
                if constexpr (use_slot_index) {
                    // skip towards left
                    std::optional<sz_t> res;
                    if (probe <= begin_slot || !(res = slot_bits.find_prev(probe - 1)).has_value() ||
                        res.value() < begin_slot) {
                        return end();
                    }
                    return const_iterator(res.value(), *this);
                } else {
                    // probe towards left
                    do {
                        if (probe == begin_slot)
                            return end();
                        probe = (probe - 1) & slot_mask;
                    } while (table[probe] >= key);
                    return const_iterator(probe, *this);
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
                return const_iterator(probe, *this);
            }
        }

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        const_iterator find_prev_iter(T key) const requires(use_simd) {
            // TODO: this doesn't work
            if (empty()) {
                return end();
            }
            key = key & value_mask;
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            bool probe_left = table[probe] >= key || !isValue(table[probe]);
            probe = probe & simd_align_mask;
            constexpr sz_t no_hit = simd_unrolled * Tvec::size();                

            if (probe_left) {
                while (true) {
                    const sz_t begin_slot = probe;
                    auto [res, in_padding] = simd_probe_rank_iter<-1, simd_pred>(probe, key, slot_mask);
                    const sz_t candidate = (begin_slot - static_cast<sz_t>(res)) & slot_mask;

                    if (res != no_hit && table[candidate] < key && isValue(table[candidate])) [[likely]] {
                        return const_iterator(static_cast<sz_t>(candidate), *this);
                    }
                    if (in_padding) [[unlikely]]
                        return end();
                }
            } else {
                const_iterator highest_pred_iter = end();
                while (true) {
                    const sz_t begin_slot = probe;
                    auto [res, in_padding] = simd_probe_rank_iter<1, simd_pred>(probe, key, slot_mask);
                    const sz_t candidate = (begin_slot + static_cast<sz_t>(res)) & slot_mask;

                    if (res != no_hit && table[candidate] < key && isValue(table[candidate])) [[likely]] {
                        highest_pred_iter = const_iterator(static_cast<sz_t>(candidate), *this);
                    }
                    if (in_padding) [[unlikely]]
                        return highest_pred_iter;
                }
            }
        }

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
                if constexpr (has_payload) {
                    return minV ? std::optional<T>(table[minV.value()]) : std::nullopt;
                } else {
                    return minV;
                }
            }
            return find_next(0);
        }

        std::optional<T> max() const requires(!dynamic) {
            if constexpr (has_payload) {
                return maxV ? std::optional<T>(table[maxV.value()]) : std::nullopt;
            } else {
                return maxV;
            }
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
                return key();
            }

            const T *operator->() const {
                return &rht.table[index];
            }

            T key() const {
                return rht.table[index];
            }

            const payload_t &payload() const requires(has_payload) {
                return rht.payload_table[index];
            }

            sz_t slot_index() const {
                return index;
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
