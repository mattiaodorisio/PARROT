#pragma once

#include <set>
#include <map>
#include <vector>
#include <random>
#include <algorithm>
#include <optional>

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

template<typename Index, bool allow_pred_equality, unsigned int bits, bool has_payload, typename... Args>
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

    std::conditional_t<has_payload, std::map<T, T>, std::set<T>> model;

    auto model_key = [](const auto& entry) -> T {
        if constexpr (has_payload) {
            return entry.first;
        } else {
            return entry;
        }
    };

    auto check_payload_for_key = [&](T key, T expected_payload) {
        if constexpr (has_payload && requires(T t) { v.find_next_iter(t); v.end(); }) {
            auto it = v.find_next_iter(key);
            check(it != v.end());
            check(it.key() == key);
            check(it.payload() == expected_payload);
        }
    };


    std::vector<std::conditional_t<has_payload, std::pair<T, T>, T>> bulk_values;
    while (model.size() < N) {
        T x = dist(rng) & (safe_shl(T(1), bits) - 1);
        if constexpr (has_payload) {
            T payload = dist(rng);
            if (model.emplace(x, payload).second) {
                bulk_values.emplace_back(x, payload);
            }
        } else {
            if (model.insert(x).second) {
                bulk_values.push_back(x);
            }
        }
    }

    if constexpr (has_payload) {
        std::sort(bulk_values.begin(), bulk_values.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
    } else {
        std::sort(bulk_values.begin(), bulk_values.end());
    }
    
    v.bulk_load(bulk_values.begin(), bulk_values.end());

    // Verify contains for all inserted elements
    for (const auto& entry: model) {
        T x = model_key(entry);
        check(v.contains(x));
        if constexpr (has_payload) {
            check_payload_for_key(x, entry.second);
        }
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
            if constexpr (has_payload) {
                T payload = dist(rng);
                if (model.emplace(x, payload).second) {
                    v.insert(x, payload);
                }
            } else {
                if (model.insert(x).second) {
                    v.insert(x);
                }
            }
        }

        // Verify contains for all inserted elements
        for (const auto& entry: model) {
            T x = model_key(entry);
            check(v.contains(x));
            if constexpr (has_payload) {
                check_payload_for_key(x, entry.second);
            }
        }
    }


    // Test find_next and find_prev on many random queries
    const int Q = std::max(N * 2, 1000);
    for (int i = 0; i < Q; ++i) {
        T q = dist(rng) & (safe_shl(T(1), bits) - 1);
        // expected next: smallest element >= q
        auto it_next = model.lower_bound(q);
        if (it_next != model.end()) {
            T expected_next = model_key(*it_next);
            check(v.find_next(q) == expected_next);
            if constexpr (has_payload) {
                auto it = v.find_next_iter(q);
                check(it != v.end());
                check(it.key() == expected_next);
                check(it.payload() == model.find(expected_next)->second);
            }
        } else {
            check(v.find_next(q) == std::nullopt);
            if constexpr (has_payload) {
                check(v.find_next_iter(q) == v.end());
            }
        }
        // expected prev: largest element < q
        auto it_prev = model.lower_bound(q + allow_pred_equality);
        if (it_prev != model.begin()) {
            --it_prev;
            T expected_prev = model_key(*it_prev);
            check(v.find_prev(q) == expected_prev);
            if constexpr (has_payload) {
                auto it = v.find_prev_iter(q);
                check(it != v.end());
                check(it.key() == expected_prev);
                check(it.payload() == model.find(expected_prev)->second);
            }
        } else {
            check(v.find_prev(q) == std::nullopt);
            if constexpr (has_payload) {
                check(v.find_prev_iter(q) == v.end());
            }
        }
    }

    std::vector<T> elems;
    if constexpr (requires(T t){ v.remove(std::forward<T>(t)); }) {
        // remove most of the elements chosen randomly
        elems.reserve(model.size());
        for (const auto& entry: model) {
            elems.push_back(model_key(entry));
        }
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
            if constexpr (has_payload && requires { it.key(); it.payload(); }) {
                auto model_it = model.find(it.key());
                check(model_it != model.end());
                check(it.key() == model_it->first);
                check(it.payload() == model_it->second);
            }
        }
        std::set<T> expected;
        for (const auto& entry: model) {
            expected.insert(model_key(entry));
        }
        check(iterated == expected);
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

template<typename Index, unsigned int bits, bool allow_pred_equality = false, bool has_payload = false, typename... Args>
void test_index(Args &&... args) {
    runForN<Index, allow_pred_equality, bits, has_payload>(0, std::forward<Args>(args)...);
    runForN<Index, allow_pred_equality, bits, has_payload>(1, std::forward<Args>(args)...);
    runForN<Index, allow_pred_equality, bits, has_payload>(10, std::forward<Args>(args)...);
    runForN<Index, allow_pred_equality, bits, has_payload>(100, std::forward<Args>(args)...);
    runForN<Index, allow_pred_equality, bits, has_payload>(1000, std::forward<Args>(args)...);
}

template<typename Index, unsigned int bits, bool allow_pred_equality = false, typename... Args>
void test_index_payload(Args &&... args) {
    test_index<Index, bits, allow_pred_equality, true>(std::forward<Args>(args)...);
}
