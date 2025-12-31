# Benchamrks CMakeLists.txt
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()
MESSAGE(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

if((CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_compile_options(-march=native)
endif()

# Bench Tiny
add_executable(bench_tiny bench_tiny.cpp)
target_link_libraries(bench_tiny PRIVATE DeLI)

# Bench RTH
add_executable(bench_rth bench_rth.cpp)
target_link_libraries(bench_rth PRIVATE DeLI)

include(FetchContent)

# Download and make Google Benchmark available automatically
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.7.1
)
# Avoid building benchmark's tests (and pulling googletest unnecessarily)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(benchmark)

target_link_libraries(bench_tiny PRIVATE benchmark::benchmark)

# Set output directory for examples
set_target_properties(bench_tiny PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bench"
)
