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
		std::vector<RHT<inner_t>> top_level;
	};
}
