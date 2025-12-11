#pragma once

#include <vector>
#include <stdexcept>
#include <limits>

namespace DeLI {
    
    template <typename T>
    class RHT_wrapper;

    template <typename T>
    class rht {
        enum search_type { binary_search, linear_search };
        enum direction { predecessor, successor }; // Optimize predecessor or successor search by changing the probing direction, TODO to be implemented
        using sz_t = size_t; // TODO check, a smaller type should be feasible for us

        T min_, max_;
        sz_t size;
        T table[1];

        // Sentinel values for empty slots
        constexpr static T empty_v = std::numeric_limits<T>::max();

        constexpr static search_type search_strategy = linear_search;
        constexpr static direction opt_dir = successor;

        sz_t scale(T key) const {
            // TODO: we can speedup the multiplication by using shifts if the range is a power of two
            // TODO: we can speedup the division by using multiplications by precomputed (see Lemire's fastmod)
            return static_cast<sz_t>(((key - min_) * (size - 1)) / (max_ - min_));
        }

    public:
        // TODO - Check if the allocation can be better implemented
        rht() = delete;
        rht(const rht&) = delete;
        rht& operator=(const rht&) = delete;
        rht(rht&&) = delete;
        rht& operator=(rht&&) = delete;
        static rht<T>* allocate_rht(sz_t size) {
            rht<T> * r = reinterpret_cast<rht<T> *>(new char[sizeof(rht) + (size - 1) * sizeof(T)]);
            // TODO: min and max will be set at construction time. For now, we set them to the full range.
            r->min_ = 0;
            r->max_ = size - 1;
            r->size = size;
            
            r->clear();
            return r;
        }

        void insert(T key) {
            sz_t probe = scale(key);
            if (probe > max_ || probe < min_) [[unlikely]] return;
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
            if (probe > max_ || probe < min_) [[unlikely]] return;

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
            for (sz_t i = 0; i < size; ++i) {
                table[i] = empty_v;
            }
        }

        bool contains(T key) const {
            sz_t probe = scale(key);
            if (probe > max_ || probe < min_) [[unlikely]] return false;

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
            for (sz_t i = 0; i < size; ++i) {
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
            if (probe > max_ || probe < min_) [[unlikely]] return empty_v;
            
            if constexpr (search_strategy == binary_search) {
                throw std::runtime_error("Not implemented");
            } else if constexpr (search_strategy == linear_search) {
                // TODO: this is naive (temporary implementation)
                while (probe < size && (table[probe] == empty_v || table[probe] < key)) {
                    ++probe;
                }
                if (probe == size) {
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
            if (probe > max_ || probe < min_) [[unlikely]] return empty_v;
            
            if constexpr (search_strategy == binary_search) {
                throw std::runtime_error("Not implemented");
            } else if constexpr (search_strategy == linear_search) {
                // TODO: this is naive (temporary implementation)
                while (probe < size && (table[probe] == empty_v || table[probe] < key)) {
                    ++probe;
                }
                do {
                    --probe;
                } while (probe != 0 && table[probe] == empty_v);
                return probe == 0 && table[probe] == empty_v ? -1 : table[probe];
            }
        }
    };

    /**
     * This is an object that contains only a pointer, it wraps an rht instance
     * Usable with standard constructor/destructor semantics
     */
    template<typename T>
    class RHT_wrapper {
        rht<T> * t;
    public:
        RHT_wrapper(const size_t sz) {
            t = ::DeLI::rht<T>::allocate_rht(sz);
        }
        ~RHT_wrapper() {
            delete[] reinterpret_cast<char *>(t);
        }
        void insert(T key) {
            t->insert(key);
        }
        void remove(T key) {
            t->remove(key);
        }
        bool contains(T key) const {
            return t->contains(key);
        }
        T find_next(T key) const {
            return t->find_next(key);
        }
        T find_prev(T key) const {
            return t->find_prev(key);
        }
    };
}
