#pragma once

#include <set>
#include <vector>
#include <random>
#include <algorithm>

#include "test_utils.h"


inline void hash_combine(std::size_t& seed, std::size_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value ^= seed + (seed << 6) + (seed >> 2);
    seed ^= value;
}

template<typename T>
std::size_t hash_value(const T& v) {
    return std::hash<std::decay_t<T>>{}(v);
}

template<typename... Args>
void hash_args(std::size_t& seed, Args&&... args) {
    (hash_combine(seed, hash_value(args)), ...);
}

template<typename Index, bool allow_pred_equality, unsigned int bits, typename... Args>
void runForN(const int N, Args &&... args) {
    using T = uint_by_bits_t<bits>;

    if constexpr (bits < 64) {
        if (N > (size_t(1) << bits)) {
            std::cout << "Skipping test with N=" << N << " bits=" << bits << " since N > 2^bits" << std::endl;
            return;
        }
    }
    std::cout << "Running test with N=" << N << " bits=" << bits << std::endl;

    std::size_t seed = 0;
    hash_combine(seed, typeid(Index).hash_code());
    hash_combine(seed, static_cast<std::size_t>(allow_pred_equality));
    hash_combine(seed, static_cast<std::size_t>(bits));
    hash_args(seed, N, args...);
    std::mt19937 rng(seed);

    Index v(std::forward<Args>(args)...);

     // deterministic seed for reproducibility
    std::uniform_int_distribution<T> dist;

    std::set<T> model;


    std::vector<T> bulk_values;
    while (model.size() < N) {
        T x = dist(rng) & (safe_shl(T(1), bits) - 1);
        if (model.insert(x).second) {
            bulk_values.push_back(x);
        }
    }
    std::sort(bulk_values.begin(), bulk_values.end());
    v.bulk_load(bulk_values.begin(), bulk_values.end());

    // Verify contains for all inserted elements
    for (T x: model) {
        check(v.contains(x));
    }

    if constexpr (requires(T t){ v.size(); }) {
        check(v.size() == model.size());
    }

    if constexpr (requires(T t){ v.insert(std::forward<T>(t)); }) {
        // remove all elements to reset
        v.clear();
        model.clear();
        // Insert N unique random values
        while (model.size() < N) {
            T x = dist(rng) & (safe_shl(T(1), bits) - 1);
            if (model.insert(x).second) {
                v.insert(x);
            }
        }

        // Verify contains for all inserted elements
        for (T x: model) {
            check(v.contains(x));
        }
    }


    // Test find_next and find_prev on many random queries
    const int Q = std::max(N * 2, 1000);
    for (int i = 0; i < Q; ++i) {
        T q = dist(rng) & (safe_shl(T(1), bits) - 1);
        // expected next: smallest element >= q
        auto it_next = model.lower_bound(q);
        if (it_next != model.end()) {
            T expected_next = *it_next;
            check(v.find_next(q) == expected_next);
        } else {
            check(v.find_next(q) == std::nullopt);
        }
        // expected prev: largest element < q
        auto it_prev = model.lower_bound(q + allow_pred_equality);
        if (it_prev != model.begin()) {
            --it_prev;
            T expected_prev = *it_prev;
            check(v.find_prev(q) == expected_prev);
        } else {
            check(v.find_prev(q) == std::nullopt);
        }
    }

    std::vector<T> elems;
    if constexpr (requires(T t){ v.remove(std::forward<T>(t)); }) {
        // remove most of the elements chosen randomly
        elems = std::vector<T>(model.begin(), model.end());
        std::shuffle(elems.begin(), elems.end(), rng);
        int remove_count = N - N / 5;
        for (int i = 0; i < remove_count; ++i) {
            T val = elems[i];
            v.remove(val);
            model.erase(val);
        }

        // After erasures, verify contains and non-contains
        for (int i = 0; i < remove_count; ++i) {
            T removed = elems[i];
            check(!v.contains(removed));
        }
        for (int i = remove_count; i < (int) elems.size(); ++i) {
            T remaining = elems[i];
            check(v.contains(remaining));
        }
        // Verify find_next/find_prev correctness on remaining set
        if (!model.empty()) {
            // test predecessors
            std::vector<T> remaining(elems.begin() + remove_count, elems.end());
            std::sort(remaining.begin(), remaining.end());
            for (size_t i = 1; i < remaining.size(); ++i) {
                T cur = remaining[i];
                T expected_prev = remaining[i - 1];
                check(v.find_prev(cur - allow_pred_equality) == expected_prev);
            }
        }
    }

    if constexpr (requires(T t){ v.begin(); }) {
        std::set<T> iterated;
        for (auto it = v.begin(); it != v.end(); ++it) {
            check(iterated.insert(*it).second); // check no duplicates
        }
        check(iterated == model);
    }


    if constexpr (requires(T t){ v.remove(std::forward<T>(t)); }) {
        // remove all remaining elements and verify the structure is empty w.r.t. contains
        for (T val: elems) {
            // some values were already removed; remove again is expected to be safe
            v.remove(val);
        }
        for (T val: elems) {
            check(!v.contains(val));
        }
    }
}

template<typename Index, unsigned int bits, bool allow_pred_equality = false, typename... Args>
void test_index(Args &&... args) {
    //runForN<Index, allow_pred_equality, bits>(0, std::forward<Args>(args)...);
    //runForN<Index, allow_pred_equality, bits>(1, std::forward<Args>(args)...);
    runForN<Index, allow_pred_equality, bits>(10, std::forward<Args>(args)...);
    runForN<Index, allow_pred_equality, bits>(100, std::forward<Args>(args)...);
    runForN<Index, allow_pred_equality, bits>(1000, std::forward<Args>(args)...);
}
