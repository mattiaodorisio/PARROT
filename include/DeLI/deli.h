#pragma once

#include <vector>
#include <variant>
#include <climits>
#include <optional>
#include <ranges>

#include "utils.h"
#include "veb.h"
#include "rht.h"

namespace DeLI { //ToDo: offset value in buckets

    template<bool dynamic, size_t low_bits, typename inner_t>
    struct bucket;

    template<size_t low_bits, typename inner_t>
    struct bucket<true, low_bits, inner_t> {
        RHT<true, low_bits> rht;
    };

    template<size_t low_bits, typename inner_t>
    struct bucket<false, low_bits, inner_t> {
        RHT<false, low_bits> rht;
        std::optional<inner_t> predecessor;
        std::optional<inner_t> successor;
    };

    template<bool dynamic, typename T, unsigned int high_bits, unsigned int value_bits = sizeof(T) * CHAR_BIT>
    class DeLI {
        static_assert(sizeof(T) * CHAR_BIT >= value_bits);
        static_assert(value_bits >= high_bits);
    private:
        using inner_t = utils::uint_by_bits_t<value_bits>;
        static constexpr size_t low_bits = value_bits - high_bits;
        static constexpr size_t buckets = size_t(1) << high_bits;

        inner_t getBucket(inner_t key) const {
            return utils::safe_shr(key, low_bits);
        }

        inner_t recombineResult(inner_t high, inner_t low) const {
            return utils::safe_shl(high, low_bits) | low;
        }

    public:

        DeLI() {
            top_level.resize(buckets);
        }


        template<typename It>
        void bulk_load(It begin, It end) {
            assert(std::is_sorted(begin, end));
            if (begin == end) {
                return;
            }
            auto inner_iter = std::ranges::subrange<It>(begin, end) |
                              std::ranges::views::transform([](T x) { return utils::to_uint<T, inner_t>(x); });

            // Split the input into buckets based on high bits
            auto bucket_start = inner_iter.begin();
            inner_t current_high = getBucket(*bucket_start);
            size_t keys_in_bucket = 0;
            std::vector<std::optional<inner_t>> bucketMaxs(dynamic ? 0 : buckets, std::nullopt);
            std::vector<std::optional<inner_t>> bucketMins(dynamic ? 0 : buckets, std::nullopt);
            for (auto it = inner_iter.begin(); it != inner_iter.end(); ++it) {
                inner_t high = getBucket(*it);
                if (high != current_high) {
                    // Bulk load the current bucket
                    top_level[current_high].rht.bulk_load(bucket_start, it, keys_in_bucket);
                    if constexpr (!dynamic) {
                        if (bucket_start != it) {
                            bucketMins[current_high] = *bucket_start;
                            bucketMaxs[current_high] = *(it - 1);
                        }
                    }
                    bucket_start = it;
                    current_high = high;
                    keys_in_bucket = 0;
                }
                keys_in_bucket++;
            }
            top_level[current_high].rht.bulk_load(bucket_start, inner_iter.end(), keys_in_bucket);
            if constexpr (!dynamic) {
                if (bucket_start != inner_iter.end()) {
                    bucketMins[current_high] = *bucket_start;
                    bucketMaxs[current_high] = *(inner_iter.end() - 1);
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
        }

        bool insert(T key_) requires(dynamic) {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            return top_level[high].rht.insert(key);
        }

        bool insert(T key_) requires(!dynamic) = delete;

        bool remove(T key_) requires(dynamic) {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            return top_level[high].rht.remove(key);
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
        std::optional<T> find_next(T key_) const {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            std::optional<inner_t> res = top_level[high].rht.find_next(key);

            if constexpr (dynamic) {
                while (!res && ++high < buckets) {
                    res = top_level[high].rht.min();
                }
            } else {
                if (!res) {
                    res = top_level[high].successor;
                    return res ? std::optional<T>(utils::from_uint<T, inner_t>(res.value())) : std::nullopt;
                }
            }

            return res ? std::optional<T>(utils::from_uint<T, inner_t>(recombineResult(high, res.value())))
                       : std::nullopt;
        }

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        std::optional<T> find_prev(T key_) const {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            std::optional<inner_t> res = top_level[high].rht.find_prev(key);

            if constexpr (dynamic) {
                while (!res && high-- > 0) {
                    res = top_level[high].rht.max();
                }
            } else {
                if (!res) {
                    res = top_level[high].predecessor;
                    return res ? std::optional<T>(utils::from_uint<T, inner_t>(res.value())) : std::nullopt;
                }
            }

            return res ? std::optional<T>(utils::from_uint<T, inner_t>(recombineResult(high, res.value())))
                       : std::nullopt;
        }

    private:
        std::vector<bucket<dynamic, low_bits, inner_t>> top_level;

        // Iterator implementation
    public:
        using inner_const_iterator = typename RHT<dynamic, low_bits>::const_iterator;

        template<typename InnerIt>
        class iterator_base {
        public:
            using pointer = decltype(std::declval<InnerIt>().operator->());

            iterator_base(const DeLI &p, bool begin) : parent(p), outer_idx(begin ? 0 : parent.top_level.size() - 1),
                                                       inner_it(begin ? parent.top_level[0].rht.begin()
                                                                      : parent.top_level[
                                                                        parent.top_level.size() - 1].rht.end()) {
                if (begin) {
                    advance_to_valid();
                }
            }

            T operator*() const {
                return utils::from_uint<T, inner_t>(parent.recombineResult(outer_idx, inner_t(*inner_it)));
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
                const std::size_t N = parent.top_level.size();
                while (outer_idx + 1 < N && inner_it == parent.top_level[outer_idx].rht.end()) {
                    ++outer_idx;
                    inner_it = parent.top_level[outer_idx].rht.begin();
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
