#pragma once

#include <vector>
#include <variant>
#include <climits>
#include <optional>
#include <ranges>

#include "utils.h"
#include "veb.h"
#include "rht.h"

namespace DeLI {


    enum class TopLevelOptimization {
        none,
        precompute,
        bucket_index
    };

    template<bool dynamic, RhtOptimization rht_opt, size_t rht_simd_unrolled, size_t rht_max_load_perc,
             TopLevelOptimization opt, typename T, unsigned int high_bits, typename PayloadT = NoPayload,
             unsigned int value_bits = sizeof(T) * CHAR_BIT>
    class DeLI {
        static_assert(value_bits < 128);
        static_assert(sizeof(T) * CHAR_BIT >= value_bits);
        static_assert(value_bits >= high_bits);
        static_assert(TopLevelOptimization::precompute != opt || !dynamic);
    private:
        using inner_t = utils::uint_by_bits_t<value_bits>;
        static constexpr size_t low_bits = value_bits - high_bits;
        static constexpr size_t buckets = size_t(1) << high_bits;
        static constexpr bool prec_pred_succ = TopLevelOptimization::precompute == opt;
        static constexpr bool use_bucket_index = TopLevelOptimization::bucket_index == opt;

        template<bool precompute, typename RHT_t>
        struct bucket;

        template<typename RHT_t>
        struct bucket<false, RHT_t> {
            RHT_t rht;
        };

        template<typename RHT_t>
        struct bucket<true, RHT_t> {
            RHT_t rht;
            std::optional<inner_t> predecessor;
            std::optional<inner_t> successor;
        };

        inner_t getBucket(inner_t key) const {
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
        std::vector<bucket<prec_pred_succ, RHT_t>> top_level;
        std::conditional_t<use_bucket_index, TwoLevelBitvector, Stub> bucket_bits;
        constexpr static bool has_payloads = RHT_t::has_payloads;
        
        template<typename Entry>
        static constexpr bool entry_has_key = requires(const Entry &e) {
            e.key();
        };

        template<typename Entry>
        static constexpr bool entry_has_first = requires(const Entry &e) {
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
                          "When has_payload is true, iterator entries must provide either .key() or .first (pair-like)");
        }

    public:

        constexpr static size_t rht_simd_width = RHT_t::simd_width;

        DeLI() : bucket_bits(buckets), top_level(buckets) {
            ;
        }


        template<typename It>
        void bulk_load(It begin, It end) {
            if constexpr (has_payloads) {
                validate_payload_iter<It>();
            }
            assert(std::is_sorted(begin, end, [](const auto &a, const auto &b) {
                return entry_to_inner(a) < entry_to_inner(b);
            }));
            if (begin == end) {
                return;
            }
            // Split the input into buckets based on high bits
            auto bucket_start = begin;
            inner_t current_high = getBucket(entry_to_inner(*bucket_start));
            size_t keys_in_bucket = 0;
            std::vector<std::optional<inner_t>> bucketMaxs(prec_pred_succ ? buckets : 0, std::nullopt);
            std::vector<std::optional<inner_t>> bucketMins(prec_pred_succ ? buckets : 0, std::nullopt);
            for (auto it = begin; it != end; ++it) {
                inner_t key_inner = entry_to_inner(*it);
                inner_t high = getBucket(key_inner);
                if (high != current_high) {
                    // Bulk load the current bucket
                    top_level[current_high].rht.bulk_load(bucket_start, it, keys_in_bucket);
                    if constexpr (prec_pred_succ) {
                        if (bucket_start != it) {
                            bucketMins[current_high] = entry_to_inner(*bucket_start);
                            bucketMaxs[current_high] = entry_to_inner(*std::prev(it));
                        }
                    }
                    bucket_start = it;
                    current_high = high;
                    keys_in_bucket = 0;
                }
                keys_in_bucket++;
            }
            top_level[current_high].rht.bulk_load(bucket_start, end, keys_in_bucket);
            if constexpr (prec_pred_succ) {
                if (bucket_start != end) {
                    bucketMins[current_high] = entry_to_inner(*bucket_start);
                    bucketMaxs[current_high] = entry_to_inner(*std::prev(end));
                }

                //precompute pred and succ of buckets
                std::optional<inner_t> last_max = std::nullopt;
                for (size_t b = 0; b < buckets; ++b) {
                    auto &bucket = top_level[b];
                    bucket.predecessor = last_max;
                    std::optional<inner_t> max = bucketMaxs[b];
                    if (max) {
                        last_max = max;
                    }
                }
                std::optional<inner_t> next_min = std::nullopt;
                for (size_t b = buckets; b-- > 0;) {
                    auto &bucket = top_level[b];
                    bucket.successor = next_min;
                    std::optional<inner_t> min = bucketMins[b];
                    if (min) {
                        next_min = min;
                    }
                }
            }
            if constexpr (use_bucket_index) {
                bucket_bits.clear();
                for (size_t b = 0; b < buckets; ++b) {
                    if (!top_level[b].rht.empty()) {
                        bucket_bits.insert(b);
                    }
                }
            }
        }

        bool insert(T key_, const typename RHT_t::payload_t &payload_ = typename RHT_t::payload_t{}) requires(dynamic) {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            if constexpr (use_bucket_index) {
                bucket_bits.insert(high);
            }
            return top_level[high].rht.insert(key, payload_);
        }

        bool insert(T key_, const typename RHT_t::payload_t &payload_ = typename RHT_t::payload_t{}) requires(!dynamic) = delete;

        bool remove(T key_) requires(dynamic) {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            bool success = top_level[high].rht.remove(key);
            if constexpr (use_bucket_index) {
                if (success && top_level[high].rht.empty())
                    bucket_bits.remove(high);
            }
            return success;
        }

        bool remove(T key_) requires(!dynamic) = delete;

        bool contains(T key_) const {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            return top_level[high].rht.contains(key);
        }

        void clear() requires(dynamic) {
            for (auto &b: top_level) {
                b.rht.clear();
            }
            if constexpr (use_bucket_index) {
                bucket_bits.clear();
            }
        }

        bool clear() requires(!dynamic) = delete;

        size_t size() const {
            size_t total_size = 0;
            for (const auto &b: top_level) {
                total_size += b.rht.size();
            }
            return total_size;
        }

        /**
        * Find successor
        * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
        */
        auto find_next_iter(T key_) const {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            auto inner_it = top_level[high].rht.find_next_iter(key);
            if (inner_it != top_level[high].rht.end()) {
                return iterator_base<inner_const_iterator>(*this, high, inner_it);
            }

            if constexpr (prec_pred_succ) {
                std::optional<inner_t> next_key = top_level[high].successor;
                if (next_key) {
                    high = getBucket(next_key.value());
                    return iterator_base<inner_const_iterator>(*this, high, top_level[high].rht.begin());
                }
            } else if constexpr (use_bucket_index) {
                if (high + 1 < buckets) {
                    std::optional<inner_t> next_bucket = bucket_bits.find_next(high + 1);
                    if (next_bucket) {
                        high = next_bucket.value();
                        return iterator_base<inner_const_iterator>(*this, high, top_level[high].rht.begin());
                    }
                }
            } else {
                while (++high < buckets) {
                    if (!top_level[high].rht.empty()) {
                        return iterator_base<inner_const_iterator>(*this, high, top_level[high].rht.begin());
                    }
                }
            }

            return end();
        }

        std::optional<T> find_next(T key_) const {
            auto it = find_next_iter(key_);
            return it == end() ? std::nullopt : std::optional<T>(it.key());
        }

        /**
        * Find predecesor iterator
        * returns iterator to the first element STRICTLY LESS than the given key
        */
        auto find_prev_iter(T key_) const {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);

            auto inner_it = top_level[high].rht.find_prev_iter(key);
            if (inner_it != top_level[high].rht.end()) {
                return iterator_base<inner_const_iterator>(*this, high, inner_it);
            }

            if constexpr (prec_pred_succ) {
                std::optional<inner_t> pred_key = top_level[high].predecessor;
                if (pred_key) {
                    high = getBucket(pred_key.value());
                    return iterator_base<inner_const_iterator>(*this, high, top_level[high].rht.max_iter());
                }
            } else if constexpr (use_bucket_index) {
                if (high > 0) {
                    std::optional<inner_t> prev_bucket = bucket_bits.find_prev(high - 1);
                    if (prev_bucket) {
                        high = prev_bucket.value();
                        auto it = top_level[high].rht.max_iter();
                        if (it != top_level[high].rht.end()) {
                            return iterator_base<inner_const_iterator>(*this, high, it);
                        }
                    }
                }
            } else {
                while (high-- > 0) {
                    auto it = top_level[high].rht.max_iter();
                    if (it != top_level[high].rht.end()) {
                        return iterator_base<inner_const_iterator>(*this, high, it);
                    }
                }
            }

            return end();
        }

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        std::optional<T> find_prev(T key_) const {
            auto it = find_prev_iter(key_);
            return it == end() ? std::nullopt : std::optional<T>(it.key());
        }


        using inner_const_iterator = typename RHT_t::const_iterator;

        template<typename InnerIt>
        class iterator_base {
        public:
            using pointer = decltype(std::declval<InnerIt>().operator->());

            iterator_base(const DeLI &p, std::size_t outer, InnerIt it) : parent(p), outer_idx(outer), inner_it(it) {
            }

            iterator_base(const DeLI &p, bool begin) : parent(p), outer_idx(begin ? 0 : parent.top_level.size() - 1),
                                                       inner_it(begin ? parent.top_level[0].rht.begin()
                                                                      : parent.top_level[
                                                                        parent.top_level.size() - 1].rht.end()) {
                if (begin) {
                    advance_to_valid();
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
                if (outer_idx < parent.top_level.size()) {
                    ++inner_it;
                    advance_to_valid();
                }
                return *this;
            }

            iterator_base operator++(int) {
                iterator_base tmp = *this;
                ++*this;
                return tmp;
            }

            bool operator==(const iterator_base &other) const {
                if (parent.top_level.size() != other.parent.top_level.size()) return false;
                if (outer_idx != other.outer_idx) return false;
                return inner_it == other.inner_it;
            }

            bool operator!=(const iterator_base &other) const { return !(*this == other); }

        private:
            const DeLI &parent;
            std::size_t outer_idx;
            InnerIt inner_it;

            void advance_to_valid() {
                if constexpr (use_bucket_index || prec_pred_succ) {
                    if (outer_idx + 1 < parent.top_level.size() && inner_it == parent.top_level[outer_idx].rht.end()) {
                        std::optional<inner_t> next_bucket;
                        if constexpr (use_bucket_index) {
                            next_bucket = parent.bucket_bits.find_next(outer_idx + 1);
                        } else if constexpr (prec_pred_succ) {
                            std::optional<inner_t> nextV = parent.top_level[outer_idx].successor;
                            next_bucket = nextV.has_value() ? std::optional<inner_t>(parent.getBucket(nextV.value()))
                                                            : std::nullopt;
                        }
                        if (next_bucket) {
                            outer_idx = next_bucket.value();
                            inner_it = parent.top_level[outer_idx].rht.begin();
                        } else {
                            // reached the end
                            outer_idx = parent.top_level.size() - 1;
                            inner_it = parent.top_level[outer_idx].rht.end();
                        }
                    }
                } else {
                    while (outer_idx + 1 < parent.top_level.size() &&
                           inner_it == parent.top_level[outer_idx].rht.end()) {
                        ++outer_idx;
                        inner_it = parent.top_level[outer_idx].rht.begin();
                    }
                }
            }
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
