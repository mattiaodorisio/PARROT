#pragma once

#include <vector>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <cstdint>

namespace DeLI {

    template<typename T>
    class RHT {
    public:
        using sz_t = size_t; // TODO check, a smaller type should be feasible for us

    private:
        std::vector<T> table;
        sz_t num_elements = 0;
        sz_t slot_shift = 0;
        sz_t begin_slot = 0; // first slot outside of wrapping area

        constexpr static sz_t valueBits = sizeof(T) * 8;
        constexpr static float th_load_factor_min = 0.2;    // TODO tune this parameter
        constexpr static float th_load_factor_max = 0.8;    // TODO tune this parameter

        // Sentinel values for empty slots
        constexpr static T empty_v = std::numeric_limits<T>::max();


        sz_t scale(T key) const {
            return key >> slot_shift;
        }

        template<typename It>
        void bulk_load(It begin, It end) {
            assert(std::is_sorted(begin, end));
            // ToDo: optimize
            for(It it = begin; it != end; ++it) {
                insert(*it);
            }
        }

        void ensure_scaling(sz_t new_num_elements) {
            float load_factor = static_cast<float>(new_num_elements) / table.size();
            if (load_factor > th_load_factor_max || load_factor < th_load_factor_min) {
                if (load_factor > th_load_factor_max) {
                    slot_shift = slot_shift - 1;
                } else {
                    slot_shift = slot_shift + 1;
                }
                std::vector<T> table_swap;
                table_swap.resize(1 << (valueBits - slot_shift), empty_v);
                std::swap(table, table_swap);
                bulk_load(table_swap.begin(), table_swap.end());
            }
        }

    public:
        RHT() {};

        void insert(T key) {
            sz_t slot_mask = table.size() - 1;
            ensure_scaling(num_elements + 1);
            sz_t probe = std::max(begin_slot, scale(key));

            while (table[probe] < key && table[probe] != empty_v) {
                probe = (probe + 1) & slot_mask;
            }
            while (table[probe] != empty_v) {
                std::swap(key, table[probe]);
                probe = (probe + 1) & slot_mask;
            }
            table[probe] = key;
            ++num_elements;
        }

        void remove(T key) {
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (table[probe] < key && table[probe] != empty_v) {
                probe = (probe + 1) & slot_mask;
            }
            if (table[probe] != key) {
                throw std::runtime_error("Key not found in remove");
            }
            // Remove the key and shift elements to fill the gap
            while (true) {
                sz_t next_probe = (probe + 1) & slot_mask;
                if (table[next_probe] == empty_v) {
                    table[probe] = empty_v;
                    break;
                }
                if (scale(table[next_probe]) == next_probe) {
                    break; // the key at next_probe is in its home position
                }
                table[probe] = table[next_probe];
                probe = next_probe;
            }
            ensure_scaling(num_elements);
        }

        bool contains(T key) const {
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (table[probe] < key && table[probe] != empty_v) {
                probe = (probe + 1) & slot_mask;
            }
            return (table[probe] == key);
        }

        void clear() {
            table.resize(0);
            table.shrink_to_fit();
        }

        bool empty() const {
            return table.empty();
        }

        inline T min() const {
            assert(!empty());
            sz_t probe = begin_slot;
            while (table[probe] == empty_v) {
                ++probe;
            }
            return table[probe];
        }

        inline T max() const {
            assert(!empty());
            sz_t slot_mask = table.size() - 1;
            sz_t probe = (begin_slot - 1) & slot_mask;
            while (table[probe] == empty_v) {
                --probe;
            }
            return table[probe];
        }

        size_t size() const {
            return num_elements;
        }

        /**
        * Find successor
        * Returns the first element NOT LESS than the given key (equivalent of std::lower_bound)
        */
        // TODO: return an iterator?
        T find_next(T key) const {
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (table[probe] == empty_v || table[probe] < key) {
                probe = (probe + 1) & slot_mask;
                if(probe==begin_slot)
                    return empty_v;
            }
            return table[probe];
        }

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        T find_prev(T key) const {
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));

            if(table[probe] >= key || table[probe] == empty_v) {
                // probe towards left
                do {
                    if(probe==begin_slot)
                        return empty_v;
                    probe = (probe - 1) & slot_mask;
                } while (table[probe] == empty_v);
                return table[probe];
            } else {
                // probe towards right
                while (table[(probe + 1) & slot_mask] < key && table[(probe + 1) & slot_mask] != empty_v &&
                       ((probe + 1) & slot_mask) != begin_slot) {
                    probe = (probe + 1) & slot_mask;
                }
                // step back to get the predecessor
                return table[probe];
            }
        }


        bool operator==(const RHT &other) const {
            if (table.size() != other.table.size())
                return false;

            for (sz_t i = 0; i < table.size(); ++i) {
                if (table[i] != other.table[i]) return false;
            }
            return true;
        }





        class iterator {
            sz_t index;
            const RHT &rht;
        public:
            iterator() : index(0), rht(*(RHT *) nullptr) {}

            iterator(sz_t start_index, const RHT &rht_ref) : index(start_index), rht(rht_ref) {
                while (index < rht.table.size() && rht.table[index] == empty_v) {
                    ++index;
                }
            }

            iterator(const iterator &other) : index(other.index), rht(other.rht) {}

            iterator(iterator &&other) noexcept: index(other.index), rht(other.rht) {}

            iterator &operator=(const iterator &other) {
                if (this == &other) return *this;
                this->~iterator();
                new(this) iterator(other);
                return *this;
            }

            iterator &operator=(iterator &&other) noexcept {
                if (this == &other) return *this;
                this->~iterator();
                new(this) iterator(std::move(other));
                return *this;
            }

            iterator &operator++() {
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

            T *operator->() const {
                return &(rht.table[index] + rht.range_min);
            }

            bool operator==(const iterator &other) const { return index == other.index; }

            bool operator!=(const iterator &other) const { return index != other.index; }
        };

        class const_iterator {
            sz_t index;
            const RHT &rht;
        public:
            const_iterator() : index(0), rht(*(const RHT *) nullptr) {}

            const_iterator(sz_t start_index, const RHT &rht_ref) : index(start_index), rht(rht_ref) {
                while (index < rht.table.size() && rht.table[index] == empty_v) {
                    ++index;
                }
            }

            const_iterator(const const_iterator &other) : index(other.index), rht(other.rht) {}

            const_iterator(const_iterator &&other) noexcept: index(other.index), rht(other.rht) {}

            const_iterator &operator=(const const_iterator &other) {
                if (this == &other) return *this;
                this->~const_iterator();
                new(this) const_iterator(other);
                return *this;
            }

            const_iterator &operator=(const_iterator &&other) noexcept {
                if (this == &other) return *this;
                this->~const_iterator();
                new(this) const_iterator(std::move(other));
                return *this;
            }

            const_iterator &operator++() {
                do {
                    ++index;
                } while (index < rht.table.size() && rht.table[index] == empty_v);
                return *this;
            }

            const_iterator operator++(int) {
                const_iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            T operator*() const {
                return rht.table[index] + rht.range_min;
            }

            const T *operator->() const {
                return &(rht.table[index] + rht.range_min);
            }

            bool operator==(const const_iterator &other) const { return index == other.index; }

            bool operator!=(const const_iterator &other) const { return index != other.index; }
        };

        iterator begin() {
            return iterator(0, *this);
        }

        iterator end() {
            return iterator(table.size(), *this);
        }

        const_iterator begin() const {
            return const_iterator(0, *this);
        }

        const_iterator end() const {
            return const_iterator(table.size(), *this);
        }

        const_iterator cbegin() const { return begin(); }

        const_iterator cend() const { return end(); }

    };
}
