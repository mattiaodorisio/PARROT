#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "parrot.h"
#include "test_index.h"


template<bool dynamic, PARROT::RhtOptimization rht_opt, PARROT::TopLevelOptimization top_opt, size_t bits, size_t lg_buckets, size_t stride>
void test_bits_payload() {
	using Index = PARROT::PARROT<dynamic, rht_opt, 2, 70, top_opt, uint_by_bits_t<bits>, lg_buckets, uint64_t, bits>;
	std::cout << "SIMD width " << Index::rht_simd_width << std::endl;
	test_index_payload<Index, bits>();
	if constexpr (bits > lg_buckets + stride) {
		test_bits_payload<dynamic, rht_opt, top_opt, bits - stride, lg_buckets, stride>();
	}
}

template<bool dynamic, PARROT::RhtOptimization rhtOpt, PARROT::TopLevelOptimization topOpt>
void run_payload() {
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 1, 60>();
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 1, 61>();
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 2, 62>();
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 8, 11>();
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 16, 63>();
}

int main() {
	std::cout << "Running PARROT payload tests" << std::endl;

	run_payload<false, PARROT::RhtOptimization::none, PARROT::TopLevelOptimization::none>();
	run_payload<true, PARROT::RhtOptimization::none, PARROT::TopLevelOptimization::none>();
	run_payload<true, PARROT::RhtOptimization::none, PARROT::TopLevelOptimization::bucket_index>();

	std::cout << "PARROT payload tests passed!" << std::endl;
	return 0;
}