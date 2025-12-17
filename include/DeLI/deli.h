#pragma once

#include <vector>
#include <variant>
#include <climits>

#include "veb.h"
#include "rht.h"

namespace DeLI {
    
    template<typename T, unsigned int low_bits>
    class DeLI {
    public:
        static constexpr int high_bits = sizeof(T) * CHAR_BIT - low_bits;

        DeLI() {
            top_level.resize(1 << high_bits);
        }

        template <typename It>
        void bulk_load(It begin, It end) {
            assert(std::is_sorted(begin, end));

            // Split the input into buckets based on high bits
            auto bucket_start = begin;
            T current_high = ((*bucket_start) >> low_bits);
            for (It it = begin; it != end; ++it) {
                T high = ((*it) >> low_bits);
                if (high != current_high) {
                    // Bulk load the current bucket
                    top_level[current_high].bulk_load(bucket_start, it, current_high << low_bits, current_high << low_bits | ((1 << low_bits) - 1));
                    bucket_start = it;
                    current_high = high;
                }
            }
            // Bulk load the last bucket
            top_level[current_high].bulk_load(bucket_start, end, current_high << low_bits, current_high << low_bits | ((1 << low_bits) - 1));
        }

        void insert(T key) {
            T high = (key >> low_bits);
            if (top_level[high].is_initialized() == false) {
                top_level[high].init(high << low_bits, high << low_bits | ((1 << low_bits) - 1));
            }
            top_level[high].insert(key);
        }

        void remove(T key) {
            T high = (key >> low_bits);
            top_level[high].remove(key);
        }

        bool contains(T key) const {
            T high = (key >> low_bits);
            return top_level[high].contains(key);
        }

        /**
         * Find successor
         * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
         */
        T find_next(T key) const {
            T high = (key >> low_bits);
            T res = top_level[high].find_next(key);
            while (res == -1 && ++high < (1 << high_bits)) {
                if (top_level[high].is_initialized())
                    res = top_level[high].min();
            }
            return res;
        }

        /**
         * Find predecesor
         * returns the first element STRICTLY LESS than the given key
         */
        T find_prev(T key) const {
            T high = (key >> low_bits);
            T res = top_level[high].find_prev(key);
            while (res == -1 && --high > 0) {
                if (top_level[high].is_initialized())
                    res = top_level[high].max();
            }
            return res;
        }

        T min() const {
          for (const auto& rht : top_level) {
            if (rht.is_initialized()) {
                return rht.min();
            }
          }
          return -1;
        }

    private:
        std::vector<RHT<T>> top_level;
    };
}
