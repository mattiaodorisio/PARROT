#pragma once

#include <vector>
#include <stdexcept>
#include <limits>

namespace DeLI {
    
    template <typename T>
    class RHT {
        enum search_type { binary_search, linear_search };
        enum direction { predecessor, successor }; // Optimize predecessor or successor search by changing the probing direction, TODO to be implemented
        using sz_t = size_t; // TODO check, a smaller type should be feasible for us

        std::vector<T> table;
        sz_t universe_size;

        // Sentinel values for empty slots
        constexpr static T empty_v = std::numeric_limits<T>::max();

        constexpr static search_type search_strategy = linear_search;
        constexpr static direction opt_dir = successor;

        sz_t scale(T key) const {
            // TODO: we can speedup the multiplication by using shifts if the range is a power of two
            // TODO: we can speedup the division by using multiplications by precomputed (see Lemire's fastmod)
            // return static_cast<sz_t>(((key) * (table.size() - 1)) / universe_size);
            return static_cast<sz_t>((1. * (key) * (table.size() - 1)) / universe_size);
        }

    public:

        RHT() = default;
        RHT(sz_t table_size, sz_t universe_size) {
            initialize(table_size, universe_size);
        }

        void initialize(sz_t table_size, sz_t universe_size) {
            table.resize(table_size, empty_v);
            this->universe_size = universe_size;
        }

        void insert(T key) {
            sz_t probe = scale(key);
            if (probe > table.size() || probe < 0) [[unlikely]] return;
            if constexpr (search_strategy == binary_search) {
                throw std::runtime_error("Not implemented");
            } else if constexpr (search_strategy == linear_search) {
                // TODO: this is naive (temporary implementation)
                while (table[probe] != empty_v && table[probe] < key) {
                    ++probe;
                }
                while (table[probe] != empty_v) {
                    std::swap(key, table[probe]);
                    ++probe;
                }
                table[probe] = key;
            }
        }

        void remove(T key) {
            sz_t probe = scale(key);
            if (probe > table.size() || probe < 0) [[unlikely]] return;
            if constexpr (search_strategy == binary_search) {
                throw std::runtime_error("Not implemented");
            } else if constexpr (search_strategy == linear_search) {
                // TODO: this is naive (temporary implementation)
                while (table[probe] != empty_v && table[probe] < key) {
                    ++probe;
                }
                if (table[probe] != key) {
                    return;
                }
                while (table[probe + 1] != empty_v && scale(table[probe + 1]) < probe + 1) {
                    table[probe] = table[probe + 1];
                    ++probe;
                }
                table[probe] = empty_v;
            }
        }

        void clear() {
            for (sz_t i = 0; i < table.size(); ++i) {
                table[i] = empty_v;
            }
        }

        bool contains(T key) const {
            sz_t probe = scale(key);
            if (probe > table.size() || probe < 0) [[unlikely]] return false;

            if constexpr (search_strategy == binary_search) {
                throw std::runtime_error("Not implemented");
            } else if constexpr (search_strategy == linear_search) {
                // TODO: this is naive (temporary implementation)
                while (table[probe] != empty_v && table[probe] < key) {
                    ++probe;
                }
                return (table[probe] == key);
            }
        }

        inline bool empty() const {
            for (sz_t i = 0; i < table.size(); ++i) {
                if (table[i] != empty_v) {
                    return false;
                }
            }
            return true;
        }

        inline T min() const { 
            throw std::runtime_error("Not implemented");
        }

        inline T max() const { 
            throw std::runtime_error("Not implemented");
        }

        /**
         * Find successor
         * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
         */
        T find_next(T key) const {
            sz_t probe = scale(key);
            if (probe > table.size() || probe < 0) [[unlikely]] return empty_v;
            
            if constexpr (search_strategy == binary_search) {
                throw std::runtime_error("Not implemented");
            } else if constexpr (search_strategy == linear_search) {
                // TODO: this is naive (temporary implementation)
                while (probe < table.size() && (table[probe] == empty_v || table[probe] < key)) {
                    ++probe;
                }
                if (probe == table.size()) {
                    return -1;
                }
                return table[probe];
            }
        }

        /**
         * Find predecesor
         * returns the first element STRICTLY LESS than the given key
         */
        T find_prev(T key) const {
            sz_t probe = scale(key);
            if (probe > table.size() || probe < 0) [[unlikely]] return empty_v;
            
            if constexpr (search_strategy == binary_search) {
                throw std::runtime_error("Not implemented");
            } else if constexpr (search_strategy == linear_search) {
                // TODO: this is naive (temporary implementation)
                while (probe < table.size() && (table[probe] == empty_v || table[probe] < key)) {
                    ++probe;
                }
                if (probe == 0) ++probe; // TODO
                do {
                    --probe;
                } while (probe != 0 && table[probe] == empty_v);
                return probe == 0 && (table[probe] == empty_v || table[probe] >= key) ? -1 : table[probe];
            }
        }
    };
}
