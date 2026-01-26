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

    template<typename T, unsigned int high_bits, unsigned int value_bits = sizeof(T) * CHAR_BIT>
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

        inner_t recombineReuslt(inner_t high, inner_t low) const {
            return utils::safe_shl(high, low_bits) | low;
        }

    public:

        DeLI() {
            top_level.resize(buckets);
        }


        template<typename It>
        void bulk_load(It begin, It end) {
            assert(std::is_sorted(begin, end));
            if(begin == end) {
                return;
            }
            auto inner_iter = std::ranges::subrange<It>(begin, end) |
                              std::ranges::views::transform([](T x) { return utils::to_uint<T, inner_t>(x); });

            // Split the input into buckets based on high bits
            auto bucket_start = inner_iter.begin();
            inner_t current_high = getBucket(*bucket_start);
            size_t keys_in_bucket = 0;
            for (auto it = inner_iter.begin(); it != inner_iter.end(); ++it) {
                inner_t high = getBucket(*it);
                if (high != current_high) {
                    // Bulk load the current bucket
                    top_level[current_high].bulk_load(bucket_start, it, keys_in_bucket);
                    bucket_start = it;
                    current_high = high;
                    keys_in_bucket = 0;
                }
                keys_in_bucket++;
            }
            top_level[current_high].bulk_load(bucket_start, inner_iter.end(), keys_in_bucket);
        }

        bool insert(T key_) {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            return top_level[high].insert(key);
        }

        bool remove(T key_) {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            return top_level[high].remove(key);
        }

        bool contains(T key_) const {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            return top_level[high].contains(key);
        }

        void clear() {
            for (auto &rht: top_level) {
                rht.clear();
            }
        }

        size_t size() const {
            size_t total_size = 0;
            for (const auto &rht: top_level) {
                total_size += rht.size();
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
            std::optional<inner_t> res = {};
            if (!top_level[high].empty()) {
                res = top_level[high].find_next(key);
            }
            while (!res && ++high < buckets) {
                if (!top_level[high].empty())
                    res = top_level[high].min();
            }
            return res ? std::optional<T>(utils::from_uint<T, inner_t>(recombineReuslt(high, res.value())))
                       : std::nullopt;
        }

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        std::optional<T> find_prev(T key_) const {
            inner_t key = utils::to_uint<T, inner_t>(key_);
            inner_t high = getBucket(key);
            std::optional<inner_t> res = {};
            if (!top_level[high].empty())
                res = top_level[high].find_prev(key);

            while (!res && high-- > 0) {
                if (!top_level[high].empty())
                    res = top_level[high].max();
            }

            return res ? std::optional<T>(utils::from_uint<T, inner_t>(recombineReuslt(high, res.value())))
                       : std::nullopt;
        }

    private:
        std::vector<RHT<low_bits>> top_level;

        // Iterator implementation
    public:
        using inner_const_iterator = typename RHT<low_bits>::const_iterator;

        template<typename InnerIt>
        class iterator_base {
        public:
            using pointer = decltype(std::declval<InnerIt>().operator->());

            iterator_base(const DeLI &p, bool begin) : parent(p), outer_idx(begin ? 0 : parent.top_level.size() - 1),
                                                       inner_it(begin ? parent.top_level[0].begin() : parent.top_level[
                                                               parent.top_level.size() - 1].end()) {
                if (begin) {
                    advance_to_valid();
                }
            }

            T operator*() const {
                return utils::from_uint<T, inner_t>(parent.recombineReuslt(outer_idx, inner_t(*inner_it)));
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
                while (outer_idx + 1 < N && inner_it == parent.top_level[outer_idx].end()) {
                    ++outer_idx;
                    inner_it = parent.top_level[outer_idx].begin();
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
