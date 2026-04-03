#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "deli.h"
#include "test_index.h"


template<bool dynamic, DeLI::RhtOptimization rht_opt, DeLI::TopLevelOptimization top_opt, size_t bits, size_t lg_buckets, size_t stride>
void test_bits_payload() {
	using Index = DeLI::DeLI<dynamic, rht_opt, 2, 70, top_opt, uint_by_bits_t<bits>, lg_buckets, uint64_t, bits>;
	std::cout << "SIMD width " << Index::rht_simd_width << std::endl;
	test_index_payload<Index, bits>();
	if constexpr (bits > lg_buckets + stride) {
		test_bits_payload<dynamic, rht_opt, top_opt, bits - stride, lg_buckets, stride>();
	}
}

template<bool dynamic, DeLI::RhtOptimization rhtOpt, DeLI::TopLevelOptimization topOpt>
void run_payload() {
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 1, 60>();
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 1, 61>();
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 2, 62>();
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 8, 11>();
	test_bits_payload<dynamic, rhtOpt, topOpt, 127, 16, 63>();
}

int main() {
	std::cout << "Running DeLI payload tests" << std::endl;

	run_payload<false, DeLI::RhtOptimization::none, DeLI::TopLevelOptimization::none>();
	run_payload<true, DeLI::RhtOptimization::none, DeLI::TopLevelOptimization::none>();
	run_payload<true, DeLI::RhtOptimization::none, DeLI::TopLevelOptimization::bucket_index>();

	std::cout << "DeLI payload tests passed!" << std::endl;
	return 0;
}