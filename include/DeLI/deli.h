#pragma once

#include <variant>
#include <climits>
#include <optional>

#include "utils.h"
#include "rht.h"

namespace DeLI {
    enum class TopLevelOptimization {
        none,
        bucket_index
    };

    template<bool dynamic, RhtOptimization rht_opt, size_t rht_simd_unrolled, size_t rht_max_load_perc,
        TopLevelOptimization opt, typename T, unsigned int high_bits, typename PayloadT = NoPayload,
        unsigned int value_bits = sizeof(T) * CHAR_BIT, template<typename k, typename v> typename top_structure =
        std::unordered_map>
    class DeLI {
        static_assert(value_bits < 128);
        static_assert(sizeof(T) * CHAR_BIT >= value_bits);
        static_assert(value_bits >= high_bits);

    private:
        using inner_t = utils::uint_by_bits_t<value_bits>;
        using top_t = utils::uint_by_bits_t<high_bits>;
        static constexpr size_t low_bits = value_bits - high_bits;
        static constexpr size_t buckets = size_t(1) << high_bits;
        static constexpr bool use_bucket_index = TopLevelOptimization::bucket_index == opt;


        top_t getBucket(inner_t key) const {
            return utils::safe_shr(key, low_bits);
        }

        inner_t recombineResult(inner_t high, inner_t low) const {
            return utils::safe_shl(high, low_bits) | low;
        }

        struct Stub {
            Stub(size_t) {
            }
        };

        using RHT_t = RHT<dynamic, low_bits, rht_opt, rht_simd_unrolled, rht_max_load_perc, PayloadT>;
        top_structure<top_t, RHT_t> top_level;
        std::conditional_t<use_bucket_index, TwoLevelBitvector, Stub> bucket_bits;
        constexpr static bool has_payloads = RHT_t::has_payloads;

        template<typename Entry>
        static constexpr bool entry_has_key = requires(const Entry &e)
        {
            e.key();
        };

        template<typename Entry>
        static constexpr bool entry_has_first = requires(const Entry &e)
        {
            e.first;
        };

        template<typename Entry>
        static inner_t entry_to_inner(const Entry &e) {
            if constexpr (entry_has_key<Entry>) {
                return utils::to_uint<T, inner_t>(e.key());
            } else if constexpr (entry_has_first<Entry>) {
                return utils::to_uint<T, inner_t>(e.first);
            } else {
                return utils::to_uint<T, inner_t>(e);
            }
        }

        template<typename It>
        static constexpr void validate_payload_iter() requires(has_payloads) {
            using entry_t = std::remove_cvref_t<decltype(*std::declval<It &>())>;
            static_assert(entry_has_key<entry_t> || entry_has_first<entry_t>,
                          "When has_payload is true, iterator entries must provide either .key() or .first (pair-like)")
                    ;
        }

    public:
        constexpr static size_t rht_simd_width = RHT_t::simd_width;

        DeLI() : bucket_bits(buckets) {
            ;
        }


        template<typename It>
        void bulk_load(It begin, It end) {
            if constexpr (has_payloads) {
                validate_payload_iter<It>();
            }
            if constexpr (use_bucket_index) {
                bucket_bits.clear();
            }
            assert(std::is_sorted(begin, end, [](const auto &a, const auto &b) {
                return entry_to_inner(a) < entry_to_inner(b);
                }));
            if (begin == end) {
                return;
            }
            // Split the input into buckets based on high bits
            auto bucket_start = begin;
            auto current_high = getBucket(entry_to_inner(*bucket_start));
            size_t keys_in_bucket = 0;
            for (auto it = begin; it != end; ++it) {
                auto key_inner = entry_to_inner(*it);
                auto high = getBucket(key_inner);
                if (high != current_high) {
                    // Bulk load the current bucket
                    top_level[current_high].bulk_load(bucket_start, it, keys_in_bucket);
                    if constexpr (use_bucket_index) {
                        bucket_bits.insert(current_high);
                    }
                    bucket_start = it;
                    current_high = high;
                    keys_in_bucket = 0;
                }
                keys_in_bucket++;
            }
            top_level[current_high].bulk_load(bucket_start, end, keys_in_bucket);
            if constexpr (use_bucket_index) {
                bucket_bits.insert(current_high);
            }
        }

        bool insert(T key_, const RHT_t::payload_t &payload_ = typename RHT_t::payload_t{}) requires(dynamic) {
            auto key = utils::to_uint<T, inner_t>(key_);
            auto high = getBucket(key);
            if constexpr (use_bucket_index) {
                bucket_bits.insert(high);
            }
            return top_level[high].insert(key, payload_);
        }

        bool insert(T key_, const RHT_t::payload_t &payload_ = typename RHT_t::payload_t{}) requires(!dynamic)
        = delete;

        bool remove(T key_) requires(dynamic) {
            auto key = utils::to_uint<T, inner_t>(key_);
            auto high = getBucket(key);
            auto bucket = top_level.find(high);
            if (bucket == top_level.end()) {
                return false;
            }
            bool success = bucket->second.remove(key);

            if (success && bucket->second.empty()) {
                top_level.erase(bucket);
                if constexpr (use_bucket_index) {
                    bucket_bits.remove(high);
                }
            }
            return success;
        }

        bool remove(T key_) requires(!dynamic) = delete;

        bool contains(T key_) const {
            auto key = utils::to_uint<T, inner_t>(key_);
            auto bucket = top_level.find(getBucket(key));
            if (bucket == top_level.end()) {
                return false;
            }
            return bucket->second.contains(key);
        }

        void clear() requires(dynamic) {
            top_level.clear();
            if constexpr (use_bucket_index) {
                bucket_bits.clear();
            }
        }

        bool clear() requires(!dynamic) = delete;

        [[nodiscard]] size_t size() const {
            size_t total_size = 0;
            for (auto &b: top_level) {
                total_size += b.second.size();
            }
            return total_size;
        }

        /**
        * Find successor
        * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
        */
        auto find_next_iter(T key_) const {
            auto key = utils::to_uint<T, inner_t>(key_);
            auto high = getBucket(key);
            auto it = top_level.find(high);
            if (it != top_level.end()) {
                auto inner_it = it->second.find_next_iter(key);
                if (inner_it != it->second.end()) {
                    return iterator_base<inner_const_iterator>(*this, high, inner_it, it->second.end());
                }
            }

            if constexpr (use_bucket_index) {
                if (high < buckets - 1) {
                    if (std::optional<inner_t> next_bucket = bucket_bits.find_next(high + 1)) {
                        high = next_bucket.value();
                        it = top_level.find(high);
                        return iterator_base<inner_const_iterator>(*this, high, it->second.begin(), it->second.end());
                    }
                }
            } else {
                for (size_t h = size_t(high) + 1; h < buckets; ++h) {
                    it = top_level.find(top_t(h));
                    if (it != top_level.end()) {
                        return iterator_base<inner_const_iterator>(*this, h, it->second.begin(), it->second.end());
                    }
                }
            }

            return end();
        }

        /**
        * Find predecesor iterator
        * returns iterator to the first element STRICTLY LESS than the given key
        */
        auto find_prev_iter(T key_) const {
            auto key = utils::to_uint<T, inner_t>(key_);
            auto high = getBucket(key);
            auto it = top_level.find(high);
            if (it != top_level.end()) {
                auto inner_it = it->second.find_prev_iter(key);
                if (inner_it != it->second.end()) {
                    return iterator_base<inner_const_iterator>(*this, high, inner_it, it->second.end());
                }
            }

            if constexpr (use_bucket_index) {
                if (high > 0) {
                    if (std::optional<inner_t> prev_bucket = bucket_bits.find_prev(high - 1)) {
                        high = prev_bucket.value();
                        it = top_level.find(high);
                        return iterator_base<inner_const_iterator>(*this, high, it->second.max_iter(), it->second.end());
                    }
                }
            } else {
                while (high-- > 0) {
                    it = top_level.find(high);
                    if (it != top_level.end()) {
                        return iterator_base<inner_const_iterator>(*this, high, it->second.max_iter(), it->second.end());
                    }
                }
            }

            return end();
        }

        /**
        * Find successor
        * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
        */
        std::optional<T> find_next(T key_) const {
            auto key = utils::to_uint<T, inner_t>(key_);
            auto high = getBucket(key);
            auto bucket = top_level.find(high);

            std::optional<inner_t> res;
            if (bucket != top_level.end()) {
                res = bucket->second.find_next(key);
            }

            if constexpr (use_bucket_index) {
                if (!res && high < buckets - 1) {
                    std::optional<inner_t> next_bucket = bucket_bits.find_next(high + 1);
                    if (next_bucket) {
                        res = top_level.find(next_bucket.value())->second.min();
                        high = next_bucket.value();
                    }
                }
            } else {
                for (size_t h = size_t(high) + 1; !res && h < buckets; ++h) {
                    bucket = top_level.find(top_t(h));
                    if (bucket != top_level.end()) {
                        res = bucket->second.min();
                        high = top_t(h);
                    }
                }
            }

            return res
                       ? std::optional<T>(utils::from_uint<T, inner_t>(recombineResult(high, res.value())))
                       : std::nullopt;
        }

        void check_consistency() {
            for (auto &b: top_level) {
                assert(!b.second.empty());
            }
        }

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        std::optional<T> find_prev(T key_) const {
            auto key = utils::to_uint<T, inner_t>(key_);
            auto high = getBucket(key);
            auto bucket = top_level.find(high);

            std::optional<inner_t> res;
            if (bucket != top_level.end()) {
                res = bucket->second.find_prev(key);
            }

            if constexpr (use_bucket_index) {
                if (!res && high > 0) {
                    std::optional<inner_t> next_bucket = bucket_bits.find_prev(high - 1);
                    if (next_bucket) {
                        res = top_level.find(next_bucket.value())->second.max();
                        high = next_bucket.value();
                    }
                }
            } else {
                while (!res && high-- > 0) {
                    bucket = top_level.find(high);
                    if (bucket != top_level.end()) {
                        res = bucket->second.max();
                    }
                }
            }

            return res
                       ? std::optional<T>(utils::from_uint<T, inner_t>(recombineResult(high, res.value())))
                       : std::nullopt;
        }


        using inner_const_iterator = typename RHT_t::const_iterator;

        template<typename InnerIt>
        class iterator_base {
        public:
            using pointer = decltype(std::declval<InnerIt>().operator->());

            iterator_base(const DeLI &p, std::size_t outer, InnerIt it, InnerIt it_end) : parent(p), outer_idx(outer),
                inner_it(it), inner_it_end(it_end) {
            }

            iterator_base(const DeLI &p, bool begin) : parent(p), outer_idx(begin ? 0 : buckets) {
                if (begin) {
                    // find the first non-empty bucket
                    if constexpr (use_bucket_index) {
                        if (std::optional<inner_t> first_bucket = parent.bucket_bits.find_next(0)) {
                            outer_idx = first_bucket.value();

                            assert(parent.top_level.contains(outer_idx));
                            auto &bucket = parent.top_level.find(outer_idx)->second;
                            inner_it = parent.top_level.find(outer_idx)->second.begin();
                            inner_it_end = parent.top_level.find(outer_idx)->second.end();
                        } else {
                            // no buckets, set to end
                            outer_idx = buckets;
                        }
                    } else {
                        while (outer_idx < buckets) {
                            auto it = parent.top_level.find(outer_idx);
                            if (it != parent.top_level.end()) {
                                inner_it = it->second.begin();
                                inner_it_end = it->second.end();
                                break;
                            }
                            ++outer_idx;
                        }
                    }

                }
            }

            T operator*() const { return key(); }

            T key() const {
                return utils::from_uint<T, inner_t>(parent.recombineResult(outer_idx, inner_t(inner_it.key())));
            }

            const typename RHT_t::payload_t &payload() const requires(has_payloads) {
                return inner_it.payload();
            }

            pointer operator->() const { return inner_it.operator->(); }

            iterator_base &operator++() {
                if (outer_idx != buckets) {
                    ++inner_it;
                    // advance to valid
                    if (inner_it == inner_it_end) {
                        ++outer_idx;
                        if constexpr (use_bucket_index) {
                            if (outer_idx < buckets) {
                                std::optional<inner_t> first_bucket = parent.bucket_bits.find_next(outer_idx);
                                if (first_bucket.has_value()) {
                                    outer_idx = first_bucket.value();
                                    assert(parent.top_level.contains(outer_idx));
                                    auto &bucket = parent.top_level.find(outer_idx)->second;
                                    inner_it = bucket.begin();
                                    inner_it_end = bucket.end();
                                } else {
                                    outer_idx = buckets;
                                }
                            }
                        } else {
                            while (outer_idx < buckets) {
                                auto it = parent.top_level.find(outer_idx);
                                if (it != parent.top_level.end()) {
                                    inner_it = it->second.begin();
                                    inner_it_end = it->second.end();
                                    return *this;
                                }
                                ++outer_idx;
                            }
                        }
                    }
                }

                return *this;
            }

            iterator_base operator++(int) {
                iterator_base tmp = *this;
                ++*this;
                return tmp;
            }

            bool operator==(const iterator_base &other) const {
                if (outer_idx != other.outer_idx) return false;
                if (outer_idx == buckets) return true; // both end
                return inner_it == other.inner_it;
            }

            bool operator!=(const iterator_base &other) const { return !(*this == other); }

        private:
            const DeLI &parent;
            std::size_t outer_idx;
            InnerIt inner_it;
            InnerIt inner_it_end;
        };

        iterator_base<inner_const_iterator> begin() const {
            return iterator_base<inner_const_iterator>(*this, true);
        }

        iterator_base<inner_const_iterator> end() const {
            return iterator_base<inner_const_iterator>(*this, false);
        }

        iterator_base<inner_const_iterator> cbegin() const { return begin(); }

        iterator_base<inner_const_iterator> cend() const { return end(); }
    };
}
