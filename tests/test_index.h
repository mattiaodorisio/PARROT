#pragma once

#include <set>
#include <vector>
#include <random>
#include <algorithm>

#include "test_utils.h"

template <typename Index, typename... Args>
void test_index(Args&&... args) {
  
  // Basic test
  {
    Index v(std::forward<Args>(args)...);
    
    // Insert
    v.insert(10);
    v.insert(20);
    v.insert(15);
    v.insert(15);
    
    // Contains
    check(v.contains(10));
    check(v.contains(15));
    check(!v.contains(5));
    
    // Next and Prev
    check(v.find_next(12) == 15);
    check(v.find_prev(18) == 15);
    check(v.find_next(15) == 15);
    check(v.find_prev(15) == 10);
    check(v.find_next(25).has_value() == false);
    check(v.find_prev(10).has_value() == false);
    
    // Remove
    v.remove(15);
    v.remove(15); // TODO: this is a workaraound: veb does not supoprt duplicates, rht does support duplicates...
    check(!v.contains(15));
    check(v.find_prev(20) == 10);
    check(v.find_next(11) == 20);
  }
  
  // Additional tests
  {
    Index v(std::forward<Args>(args)...);
    
    std::mt19937 rng(12345); // deterministic seed for reproducibility
    std::uniform_int_distribution<int> dist(0, 100000);
    
    std::set<int> model;
    const int N = 1000;
    // Insert N unique random values
    while (model.size() < N) {
      int x = dist(rng);
      if (model.insert(x).second) {
        v.insert(x);
      }
    }
    
    // Verify contains for all inserted elements
    for (int x : model) {
      check(v.contains(x));
    }
    
    // Test find_next and find_prev on many random queries
    const int Q = 300;
    for (int i = 0; i < Q; ++i) {
      int q = dist(rng);
      // expected next: smallest element > q
      auto it_next = model.lower_bound(q);
      if (it_next != model.end()) {
        int expected_next = *it_next;
        check(v.find_next(q) == expected_next);
      }
      // expected prev: largest element < q
      auto it_prev = model.lower_bound(q);
      if (it_prev != model.begin()) {
        --it_prev;
        int expected_prev = *it_prev;
        check(v.find_prev(q) == expected_prev);
      }
    }
    
    // remove half of the elements chosen randomly
    std::vector<int> elems(model.begin(), model.end());
    std::shuffle(elems.begin(), elems.end(), rng);
    int remove_count = N / 2;
    for (int i = 0; i < remove_count; ++i) {
      int val = elems[i];
      v.remove(val);
      model.erase(val);
    }

    // After erasures, verify contains and non-contains
    for (int i = 0; i < remove_count; ++i) {
      int removed = elems[i];
      check(!v.contains(removed));
    }
    for (int i = remove_count; i < (int)elems.size(); ++i) {
      int remaining = elems[i];
      check(v.contains(remaining));
    }
    
    // Verify find_next/find_prev correctness on remaining set
    if (!model.empty()) {
      // test predecessors
      std::vector<int> remaining(elems.begin() + remove_count, elems.end());
      std::sort(remaining.begin(), remaining.end());
      for (size_t i = 1; i < remaining.size(); ++i) {
        int cur = remaining[i];
        int expected_prev = remaining[i-1];
        check(v.find_prev(cur) == expected_prev);
      }
    }
    
    // remove all remaining elements and verify the structure is empty w.r.t. contains
    for (int val : elems) {
      // some values were already removed; remove again is expected to be safe
      v.remove(val);
    }
    for (int val : elems) {
      check(!v.contains(val));
    }
  }

#if false
  // Test benchmark-like
  {
    Index v(std::forward<Args>(args)...);
    
    const size_t NUM_KEYS = 1 << 20;
    const size_t NUM_QUERIES = 1 << 20;
    std::vector<int> keys(NUM_KEYS), queries(NUM_QUERIES);
    for (size_t i = 0; i < NUM_KEYS; ++i) {
      keys[i] = i * 2654435761 % NUM_KEYS;
    }
    for (size_t i = 0; i < NUM_QUERIES; ++i) {
      queries[i] = i * 40503 % NUM_QUERIES;
    }
    
    std::sort(keys.begin(), keys.end());
    // for (size_t i = 0; i < NUM_KEYS; ++i) {
    //   v.insert(keys[i]);
    // }
    v.bulk_load(keys.begin(), keys.end());

    for (size_t i = 0; i < NUM_QUERIES; ++i) {
      auto result = v.find_next(queries[i]);
      auto lb = std::lower_bound(keys.begin(), keys.end(), queries[i]);
      check(result == (lb == keys.end() ? -1 : *lb));
    }
  }
#endif
}
