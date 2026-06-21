cmake_minimum_required(VERSION 3.12)
project(PARROT VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the PARROT library as header-only interface library
add_library(PARROT INTERFACE)
target_include_directories(PARROT INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/PARROT>
    $<INSTALL_INTERFACE:include/PARROT>
)

# Enable testing (must be before add_subdirectory with tests)
enable_testing()

# Add subdirectories
add_subdirectory(examples)
add_subdirectory(tests)

# Installation (for use as submodule)
install(DIRECTORY include/ DESTINATION include)
install(TARGETS PARROT EXPORT PARROTTargets)
install(EXPORT PARROTTargets
    FILE PARROTTargets.cmake
    DESTINATION lib/cmake/PARROT
)

# Create config file for find_package
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PARROTConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/PARROTConfig.cmake"
    INSTALL_DESTINATION lib/cmake/PARROT
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PARROTConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PARROTConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PARROTConfigVersion.cmake"
    DESTINATION lib/cmake/PARROT
)


add_executable(bench_tiny benchmarks/bench_tiny.cpp)
target_link_libraries(bench_tiny PRIVATE PARROT)
