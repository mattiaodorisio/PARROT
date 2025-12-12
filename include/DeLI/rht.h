#pragma once

#include <vector>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cassert>
#include <iterator>

namespace DeLI {
    
    template <typename T>
    class RHT {
        enum search_type { binary_search, linear_search };
        enum direction { predecessor, successor }; // Optimize predecessor or successor search by changing the probing direction, TODO to be implemented
        using sz_t = size_t; // TODO check, a smaller type should be feasible for us

        std::vector<T> table;
        sz_t range_min, range_max; // TODO - we possibly don't need both
        sz_t sum_displacement = 0;
        sz_t max_displacement = 0;
        sz_t num_elements = 0;

        // Table parameters
        constexpr static sz_t th_max_displacement = 16; // TODO tune this parameter
        constexpr static sz_t th_avg_displacement = 8; // TODO tune this parameter
        constexpr static sz_t oversize_factor = 2; // TODO tune this parameter

        // Sentinel values for empty slots
        constexpr static T empty_v = std::numeric_limits<T>::max();

        // Search parameters
        constexpr static search_type search_strategy = linear_search;
        constexpr static direction opt_dir = successor;

        sz_t scale(T key) const {
            // TODO: we can speedup the multiplication by using shifts if the range is a power of two
            // TODO: we can speedup the division by using multiplications by precomputed (see Lemire's fastmod)
            // return static_cast<sz_t>(((key) * (table.size() - 1)) / universe_size);
            return static_cast<sz_t>((1. * (key) * (table.size() - 1 - th_max_displacement)) / (range_max - range_min));
        }

    public:

        RHT() {};

        RHT(sz_t expected_size, sz_t range_min, sz_t range_max) {
            init(expected_size, range_min, range_max);
        }

        void init() {
            init(0, std::numeric_limits<T>::max());
        }

        void init(sz_t range_min, sz_t range_max, sz_t expected_size = th_max_displacement) {
            this->range_min = range_min;
            this->range_max = range_max;
            table.resize(oversize_factor * expected_size + th_max_displacement, empty_v);
        }

        template <typename It>
        bool bulk_load(It begin, It end, sz_t range_min, sz_t range_max) {
            clear();
            sz_t n = std::distance(begin, end);
            this->range_min = range_min;
            this->range_max = range_max;
            table.resize(oversize_factor * n + th_max_displacement, empty_v);

            if (std::is_sorted(begin, end) == false) {
                for (It it = begin; it != end; ++it) {
                    insert(*it);
                }
            } else {
                num_elements = std::distance(begin, end);
                sz_t probe_index = 0; // The leftmost free slot in the table
                for (It it = begin; it != end; ++it) {
                    T key = *it - range_min;
                    sz_t probe = scale(key);
                    if (probe > table.size() || probe < 0) [[unlikely]] throw std::runtime_error("Key out of range in bulk_load");
                    if (probe > probe_index) {
                        probe_index = probe;
                    }
                    sum_displacement += probe_index - probe;
                    max_displacement = std::max(max_displacement, probe_index - probe);
                    table[probe_index++] = key;
                }
            }

            // TODO ADD check for the gaps
            if (max_displacement > th_max_displacement || average_displacement() > th_avg_displacement) {
                // throw std::runtime_error("Displacement too high in bulk_load");
                // table.resize(0);
                // table.shrink_to_fit();
                // return false;
            }
            return true;
        }

        void insert(T key) {
            key -= range_min;
            sz_t probe = scale(key);
            if (probe > table.size() || probe < 0) [[unlikely]] throw std::runtime_error("Key out of range in insert");
            if constexpr (search_strategy == binary_search) {
                throw std::runtime_error("Not implemented");
            } else if constexpr (search_strategy == linear_search) {
                // TODO: this is naive (temporary implementation)
                while (table[probe] != empty_v && table[probe] < key && probe < table.size()) {
                    ++probe;
                    ++sum_displacement;
                }
                while (table[probe] != empty_v && probe < table.size()) {
                    std::swap(key, table[probe]);
                    ++probe;
                    ++sum_displacement;
                }

                if (probe >= table.size()) [[unlikely]] { // TODO improve implementation
                    std::vector<T> old_data;
                    old_data.reserve(num_elements + 1);
                    std::copy(begin(), end(), std::back_inserter(old_data));
                    auto new_key = std::upper_bound(old_data.begin(), old_data.end(), key + range_min);
                    old_data.insert(new_key, key + range_min);
                    assert(std::is_sorted(old_data.begin(), old_data.end()));
                    bulk_load(old_data.begin(), old_data.end(), range_min, range_max);
                } else {
                    table[probe] = key;
                    ++num_elements;
                }
            }
        }

        void remove(T key) {
            key -= range_min;
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
                    --sum_displacement;
                }
                table[probe] = empty_v;
                --num_elements;
            }
        }

        void clear() {
            for (sz_t i = 0; i < table.size(); ++i) {
                table[i] = empty_v;
            }
            sum_displacement = 0;
            max_displacement = 0;
            range_min = range_max = 0;
            num_elements = 0;
        }

        void free_memory() {
            table.resize(0);
            table.shrink_to_fit();
            sum_displacement = 0;
            max_displacement = 0;
            range_min = range_max = 0;
            num_elements = 0;
        }

        bool is_initialized() const {
            return range_min != range_max;
        }

        bool contains(T key) const {
            key -= range_min;
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
            return find_next(range_min);
        }

        inline T max() const { 
            return find_prev(range_max);
        }

        /**
         * Find successor
         * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
         */
        T find_next(T key) const {
            key -= range_min;
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
                return table[probe] + range_min;
            }
        }

        /**
         * Find predecesor
         * returns the first element STRICTLY LESS than the given key
         */
        T find_prev(T key) const {
            key -= range_min;
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
                return probe == 0 && (table[probe] == empty_v || table[probe] >= key) ? -1 : table[probe] + range_min;
            }
        }

        inline double average_displacement() const {
            return static_cast<double>(sum_displacement) / static_cast<double>(num_elements);
        }

        inline sz_t maximum_displacement() const {
            return max_displacement;
        }

        bool operator==(const RHT& other) const {
            if (table.size() != other.table.size() || 
                num_elements != other.num_elements ||
                range_min != other.range_min ||
                range_max != other.range_max)
                return false;

            for (sz_t i = 0; i < table.size(); ++i) {
                if (table[i] != other.table[i]) return false;
            }
            return true;
        }

        // Iterator for RHT
        template <typename Ti>
        class iterator {
            using iterator_category = std::forward_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = Ti;
            using pointer           = Ti*;
            using reference         = Ti&;  // or also value_type&

            sz_t index;
            const RHT& rht;
        public:
            /*
            If the invariant on the gaps is preserved, all the following functions takes constant time
            */
            iterator() : index(0), rht(*(RHT*)nullptr) {}
            iterator(sz_t start_index, const RHT& rht_ref) : index(start_index), rht(rht_ref) {
                while (index < rht.table.size() && rht.table[index] == empty_v) {
                    ++index;
                }
            }

            iterator(const iterator& other) : index(other.index), rht(other.rht) {}
            iterator(iterator&& other) noexcept : index(other.index), rht(other.rht) {}
            iterator& operator=(const iterator& other) {
                if (this == &other) return *this;
                this->~iterator();
                new (this) iterator(other);
                return *this;
            }
            iterator& operator=(iterator&& other) noexcept {
                if (this == &other) return *this;
                this->~iterator();
                new (this) iterator(std::move(other));
                return *this;
            }

            iterator& operator++() {
                do {
                    ++index;
                } while (index < rht.table.size() && rht.table[index] == empty_v);
                return *this;
            }
            iterator operator++(int) {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            T operator*() const {
                return rht.table[index] + rht.range_min;
            }
            T* operator->() const {
                return &(rht.table[index] + rht.range_min);
            }

            bool operator==(const iterator& other) const { return index == other.index; }
            bool operator!=(const iterator& other) const { return index != other.index; }
        };

        iterator<T> begin() const {
            return iterator(0, *this);
        }

        iterator<T> end() const {
            return iterator(table.size(), *this);
        }
    };
}
