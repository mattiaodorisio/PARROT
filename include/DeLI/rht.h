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
        const sz_t value_bits = 0; // number of bits of the value that are used, might be constexp in future

        constexpr static int slot_factor = 2;

        // Sentinel values for empty slots
        constexpr static T empty_v = std::numeric_limits<T>::max();


        sz_t scale(T key) const {
            return key >> slot_shift;
        }

        template<typename It>
        void bulk_load(It begin, It end) {
            assert(std::is_sorted(begin, end));
            // ToDo: optimize
            for (It it = begin; it != end; ++it) {
                if (*it != empty_v)
                    insert(*it);
            }
        }

        sz_t num_slot_target(sz_t num_elements) const {
            //trick to make 0 map to 0
            return std::bit_ceil(2 * slot_factor * num_elements) >> 1;
        }

        void ensure_scaling(sz_t new_num_elements) {
            sz_t slot_target = num_slot_target(new_num_elements);
            if (table.size() < slot_target || table.size() > slot_target * 4) {
                std::vector<T> table_swap;
                table_swap.resize(slot_target, empty_v);
                std::swap(table, table_swap);
                slot_shift = value_bits - static_cast<sz_t>(std::countr_zero(slot_target));
                bulk_load(table_swap.begin(), table_swap.end());
            }
        }

    public:
        RHT() {};

        RHT(sz_t value_bits) : value_bits(value_bits) {
        }

        void insert(T key) {
            sz_t slot_mask = table.size() - 1;
            ensure_scaling(num_elements + 1);
            sz_t probe = std::max(begin_slot, scale(key));
            sz_t start_probe = probe;

            while (table[probe] < key && table[probe] != empty_v) {
                probe = (probe + 1) & slot_mask;
            }
            while (table[probe] != empty_v) {
                std::swap(key, table[probe]);
                probe = (probe + 1) & slot_mask;
                if (probe == begin_slot) {
                    begin_slot++;
                }
            }
            table[probe] = key;
            ++num_elements;
            if (probe < start_probe) // wrapped around
                begin_slot++;
        }

        bool remove(T key) {
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (table[probe] < key && table[probe] != empty_v) {
                probe = (probe + 1) & slot_mask;
            }
            if (table[probe] != key) {
                return false;
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
            num_elements--;
            ensure_scaling(num_elements);
            return true;
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
        std::optional<T> find_next(T key) const {
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));
            while (table[probe] == empty_v || table[probe] < key) {
                probe = (probe + 1) & slot_mask;
                if (probe == begin_slot)
                    return std::nullopt;
            }
            return table[probe];
        }

        /**
        * Find predecesor
        * returns the first element STRICTLY LESS than the given key
        */
        std::optional<T> find_prev(T key) const {
            sz_t slot_mask = table.size() - 1;
            sz_t probe = std::max(begin_slot, scale(key));

            if (table[probe] >= key || table[probe] == empty_v) {
                // probe towards left
                do {
                    if (probe == begin_slot)
                        return std::nullopt;
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
                if (rht.empty()) {
                    index = sz_t(-1);
                    return;
                }
                while (rht.table[index] == empty_v) {
                    index = (index + 1) & (rht.table.size() - 1);
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
                while (true) {
                    index = (index + 1) & (rht.table.size() - 1);
                    if (index == rht.begin_slot) {
                        index = sz_t(-1);
                        return *this;
                    }
                    if(rht.table[index] != empty_v) {
                        return *this;
                    }
                };
            }

            iterator operator++(int) {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            T operator*() const {
                return rht.table[index];
            }

            T *operator->() const {
                return &rht.table[index];
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
                if (rht.empty()) {
                    index = sz_t(-1);
                    return;
                }
                while (rht.table[index] == empty_v) {
                    index = (index + 1) & (rht.table.size() - 1);
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
                while (true) {
                    index = (index + 1) & (rht.table.size() - 1);
                    if (index == rht.begin_slot) {
                        index = sz_t(-1);
                        return *this;
                    }
                    if(rht.table[index] != empty_v) {
                        return *this;
                    }
                };
            }

            const_iterator operator++(int) {
                const_iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            T operator*() const {
                return rht.table[index];
            }

            const T *operator->() const {
                return &rht.table[index];
            }

            bool operator==(const const_iterator &other) const { return index == other.index; }

            bool operator!=(const const_iterator &other) const { return index != other.index; }
        };

        iterator begin() {
            return iterator(begin_slot, *this);
        }

        iterator end() {
            return iterator(sz_t(-1), *this);
        }

        const_iterator begin() const {
            return const_iterator(begin_slot, *this);
        }

        const_iterator end() const {
            return const_iterator(sz_t(-1), *this);
        }

        const_iterator cbegin() const { return begin(); }

        const_iterator cend() const { return end(); }

    };
}
