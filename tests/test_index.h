#pragma once

#include <set>
#include <vector>
#include <random>
#include <algorithm>

#include "test_utils.h"


template<typename Index, unsigned int bits, typename... Args>
void runForN(const int N, Args &&... args) {
    using T = DeLI::utils::uint_by_bits_t<bits>;

    if constexpr (bits < 64) {
        if(N > (size_t(1) << bits)) {
            std::cout << "Skipping test with N=" << N << " bits=" << bits << " since N > 2^bits" << std::endl;
            return;
        }
    }
    std::cout << "Running test with N=" << N << " bits=" << bits << std::endl;

    Index v(std::forward<Args>(args)...);

    std::mt19937 rng(N * bits); // deterministic seed for reproducibility
    std::uniform_int_distribution<T> dist;

    std::set<T> model;

// Test bulk load if supported
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

// Test find_next and find_prev on many random queries
    const int Q = std::max(N * 2, 1000);
    for (int i = 0; i < Q; ++i) {
        T q = dist(rng) & (safe_shl(T(1), bits) - 1);
// expected next: smallest element > q
        auto it_next = model.lower_bound(q);
        if (it_next != model.end()) {
            T expected_next = *it_next;
            check(v.find_next(q) == expected_next);
        }
// expected prev: largest element < q
        auto it_prev = model.lower_bound(q);
        if (it_prev != model.begin()) {
            --it_prev;
            T expected_prev = *it_prev;
            check(v.find_prev(q) == expected_prev);
        }
    }

// remove most of the elements chosen randomly
    std::vector<T> elems(model.begin(), model.end());
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

// verfify iterators reflect current state
    {
        std::set<T> iterated;
        for (auto it = v.begin(); it != v.end(); ++it) {
            iterated.insert(*it);
        }
        check(iterated == model);
    }

// verify const iterators reflect current state
    {
        const Index &cv = v;
        std::set<T> iterated;
        for (auto it = cv.begin(); it != cv.end(); ++it) {
            iterated.insert(*it);
        }
        check(iterated == model);
    }

// Verify find_next/find_prev correctness on remaining set
    if (!model.empty()) {
// test predecessors
        std::vector<T> remaining(elems.begin() + remove_count, elems.end());
        std::sort(remaining.begin(), remaining.end());
        for (size_t i = 1; i < remaining.size(); ++i) {
            T cur = remaining[i];
            T expected_prev = remaining[i - 1];
            check(v.find_prev(cur) == expected_prev);
        }
    }

// remove all remaining elements and verify the structure is empty w.r.t. contains
    for (T val: elems) {
// some values were already removed; remove again is expected to be safe
        v.remove(val);
    }
    for (T val: elems) {
        check(!v.contains(val));
    }
}

template<typename Index, unsigned int bits, typename... Args>
void test_index(Args &&... args) {
    // Additional tests
    runForN<Index, bits>(0, std::forward<Args>(args)...);
    runForN<Index, bits>(1, std::forward<Args>(args)...);
    runForN<Index, bits>(10, std::forward<Args>(args)...);
    runForN<Index, bits>(1000, std::forward<Args>(args)...);
}
