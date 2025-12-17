#pragma once

#include <vector>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <cstdint>

namespace DeLI {
  
  class bitvector {
    using sz_t = size_t;
    
    std::vector<uint64_t> vector;
    sz_t num_bits;
    
    public:
    bitvector() : num_bits(0) {}
    
    bitvector(sz_t size) : num_bits(size) {
      vector.resize((size + 63) / 64, 0);
    }
    
    void resize(sz_t size) {
      num_bits = size;
      vector.resize((size + 63) / 64, 0);
    }
    
    void set(sz_t index) {
      vector[index / 64] |= (1ULL << (index % 64));
    }
    
    bool get(sz_t index) const {
      return (vector[index / 64] >> (index % 64)) & 1ULL;
    }
    
    void clear(sz_t index) {
      vector[index / 64] &= ~(1ULL << (index % 64));
    }
    
    sz_t find_next_set(sz_t index) const {
      // TODO: check if we can do better (avx simd)
      sz_t vec_index = index / 64;
      sz_t bit_index = index % 64;
      
      // Check the current 64-bit block
      uint64_t block = vector[vec_index] >> bit_index;
      if (block != 0) {
        return index + __builtin_ctzll(block);
      }
      
      // Check subsequent blocks
      for (sz_t i = vec_index + 1; i < vector.size(); ++i) {
        if (vector[i] != 0) {
          return i * 64 + __builtin_ctzll(vector[i]);
        }
      }
      
      return num_bits; // No set bit found
    }
    
    void free_memory() {
      vector.clear();
      vector.shrink_to_fit();
      num_bits = 0;
    }
  };
  
  // XOR bitvector
  // This take an arbitrary vector and creates a two layer bitvector that has bit set for non-empty blocks
  template <typename T, T zero_value, size_t block_size = 64>
  class xor_bitvector {
    using sz_t = size_t;
    
    bitvector top_level;
    bitvector bottom_level;
    sz_t num_elements;
    
    public:
    xor_bitvector() : num_elements(0) {}
    
    template <typename It>
    void build(const It begin, const It end) {
      num_elements = std::distance(begin, end);
      sz_t num_blocks = (num_elements + block_size - 1) / block_size;
      top_level.resize(num_blocks);
      bottom_level.resize(num_elements);
      
      // TODO XXX: improve it (word operations, simd, etc.)
      for (sz_t i = 0; i < num_elements; ++i) {
        if (begin[i] != zero_value) {
          bottom_level.set(i);
          top_level.set(i / block_size);
        }
      }
    }
    
    sz_t find_next_set(sz_t index) const {
      sz_t block_index = index / block_size;
      sz_t within_block_index = index % block_size;
      
      // Find the next block with a non-zero element
      sz_t next_block = top_level.find_next_set(block_index);
      if (next_block >= (num_elements + block_size - 1) / block_size) {
        return num_elements; // No more non-zero elements
      }
      
      // Now find the next non-zero element within that block
      sz_t start_index = next_block * block_size;
      if (next_block == block_index) {
        start_index += within_block_index;
      }
      
      return bottom_level.find_next_set(start_index);
    }
    
    void set(sz_t index) {
      bottom_level.set(index);
      top_level.set(index / block_size);
    }
    
    bool get(sz_t index) const {
      return bottom_level.get(index);
    }
    
    void free_memory() {
      top_level.free_memory();
      bottom_level.free_memory();
      num_elements = 0;
    }
  };
};
