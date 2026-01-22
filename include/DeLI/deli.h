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
	
	template<typename T, unsigned int low_bits>
	class DeLI {
	public:
		using value_type = T;
		using inner_t = std::conditional_t<(sizeof(T) <= 4), uint32_t, uint64_t>;
		static_assert(sizeof(inner_t) == 4 || sizeof(inner_t) == 8, "RHT only supports 32 or 64 bit keys, to be implemented the support for other data types");

		static constexpr int high_bits = sizeof(T) * CHAR_BIT - low_bits;
		
		DeLI() {
			top_level.resize(1 << high_bits);
		}
		
		template <typename It>
		void bulk_load(It begin, It end) {
			assert(std::is_sorted(begin, end));
			auto inner_iter = std::ranges::subrange<It>(begin, end) | std::ranges::views::transform([](T x) { return utils::to_uint<T, inner_t>(x); });

			// Split the input into buckets based on high bits
			auto bucket_start = inner_iter.begin();
			inner_t current_high = (*bucket_start) >> low_bits;
			for (auto it = inner_iter.begin(); it != inner_iter.end(); ++it) {
				inner_t high = (*it) >> low_bits;
				if (high != current_high) {
					// Bulk load the current bucket
					top_level[current_high].init(current_high << low_bits, current_high << low_bits | ((1 << low_bits) - 1));
					top_level[current_high].bulk_load(bucket_start, it);
					bucket_start = it;
					current_high = high;
				}
			}
			// Bulk load the last bucket
			top_level[current_high].init(current_high << low_bits, current_high << low_bits | ((1 << low_bits) - 1));
			top_level[current_high].bulk_load(bucket_start, inner_iter.end());
		}
		
		void insert(T key_) {
			inner_t key = utils::to_uint<T, inner_t>(key_);
			inner_t high = key >> low_bits;
			if (top_level[high].is_initialized() == false) {
				top_level[high].init(high << low_bits, high << low_bits | ((1 << low_bits) - 1));
			}
			top_level[high].insert(key);
		}

		void remove(T key_) {
			inner_t key = utils::to_uint<T, inner_t>(key_);
			inner_t high = key >> low_bits;
			top_level[high].remove(key);
		}

		bool contains(T key_) const {
			inner_t key = utils::to_uint<T, inner_t>(key_);
			inner_t high = key >> low_bits;
			return top_level[high].contains(key);
		}

		void clear() {
			for (auto& rht : top_level) {
				rht.clear();
			}
		}

		size_t size() const {
			size_t total_size = 0;
			for (const auto& rht : top_level) {
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
			inner_t high = (key >> low_bits);
			std::optional<inner_t> res = {};
			if (top_level[high].is_initialized()) {
				res = top_level[high].find_next(key);
			}
			while (!res && ++high < (1 << high_bits)) {
				if (top_level[high].is_initialized())
					res = top_level[high].min();
			}
			return res ? std::optional<T>(utils::from_uint<T, inner_t>(res.value())) : std::nullopt;
		}
		
		/**
		* Find predecesor
		* returns the first element STRICTLY LESS than the given key
		*/
		std::optional<T> find_prev(T key_) const {
			inner_t key = utils::to_uint<T, inner_t>(key_);
			inner_t high = (key >> low_bits);
			std::optional<inner_t> res = {};
			if (top_level[high].is_initialized())
				res = top_level[high].find_prev(key);

			while (!res && --high > 0) {
				if (top_level[high].is_initialized())
					res = top_level[high].max();
			}

			// Possible last iteration
			if (!res) {
				if (top_level[high].is_initialized())
					res = top_level[high].max();
			}

			return res ? std::optional<T>(utils::from_uint<T, inner_t>(res.value())) : std::nullopt;
		}

		std::optional<T> min() const {
			for (const auto& rht : top_level) {
				if (rht.is_initialized()) {
					auto res = rht.min();
					return res ? std::optional<T>(utils::from_uint<T, inner_t>(res.value())) : std::nullopt;
				}
			}
			return std::nullopt;
		}
		
		private:
		std::vector<RHT<T>> top_level;

    // Iterator implementation
    public:
        using inner_iterator = typename RHT<T>::iterator;
        using inner_const_iterator = typename RHT<T>::const_iterator;

        template <typename ParentPtr, typename InnerIt>
        class iterator_base {
        public:
            using reference = decltype(*std::declval<InnerIt>());
            using pointer = decltype(std::declval<InnerIt>().operator->());

            iterator_base() : parent(nullptr), outer_idx(0), inner_it() {}
            iterator_base(ParentPtr p, std::size_t idx, InnerIt it) : parent(p), outer_idx(idx), inner_it(it) {
                if (parent) advance_to_valid();
            }

            reference operator*() const { return *inner_it; }
            pointer operator->() const { return inner_it.operator->(); }

            iterator_base& operator++() {
                if (parent && outer_idx < parent->top_level.size()) {
                    ++inner_it;
                    advance_to_valid();
                }
                return *this;
            }

            iterator_base operator++(int) { iterator_base tmp = *this; ++*this; return tmp; }

            bool operator==(const iterator_base& other) const {
                if (parent != other.parent) return false;
                if (!parent) return true;
                std::size_t N = parent->top_level.size();
                if (outer_idx == N && other.outer_idx == N) return true; // both end()
                return outer_idx == other.outer_idx && inner_it == other.inner_it;
            }
            bool operator!=(const iterator_base& other) const { return !(*this == other); }

        private:
            ParentPtr parent;
            std::size_t outer_idx;
            InnerIt inner_it;

			void advance_to_valid() {
				const std::size_t N = parent->top_level.size();
				while (outer_idx < N) {
					auto &r = parent->top_level[outer_idx]; // use auto& so constness follows ParentPtr
					if (r.is_initialized()) {
						InnerIt b = r.begin();
						InnerIt e = r.end();
						if (inner_it == InnerIt()) inner_it = b; // first time entering this bucket
						if (inner_it != e) return;               // valid element
					}
					++outer_idx;
					inner_it = InnerIt();
				}
				// reached end: leave outer_idx == N, inner_it default
			}
        };

        using iterator = iterator_base<DeLI*, inner_iterator>;
        using const_iterator = iterator_base<const DeLI*, inner_const_iterator>;

        iterator begin() {
            std::size_t N = top_level.size();
            for (std::size_t i = 0; i < N; ++i) {
                if (top_level[i].is_initialized()) {
                    auto b = top_level[i].begin();
                    auto e = top_level[i].end();
                    if (b != e) return iterator(this, i, b);
                }
            }
            return end();
        }

        iterator end() {
            return iterator(this, top_level.size(), inner_iterator());
        }

        const_iterator begin() const {
            std::size_t N = top_level.size();
            for (std::size_t i = 0; i < N; ++i) {
                if (top_level[i].is_initialized()) {
                    auto b = top_level[i].begin();
                    auto e = top_level[i].end();
                    if (b != e) return const_iterator(this, i, b);
                }
            }
            return cend();
        }

        const_iterator end() const {
            return const_iterator(this, top_level.size(), inner_const_iterator());
        }

        const_iterator cbegin() const { return begin(); }
        const_iterator cend() const { return end(); }
	};
}
