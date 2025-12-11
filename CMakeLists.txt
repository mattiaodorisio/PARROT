cmake_minimum_required(VERSION 3.12)
project(DeLI VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the DeLI library as header-only interface library
add_library(DeLI INTERFACE)
target_include_directories(DeLI INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/DeLI>
    $<INSTALL_INTERFACE:include/DeLI>
)

# Enable testing (must be before add_subdirectory with tests)
enable_testing()

# Add subdirectories
add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(benchmarks)

# Installation (for use as submodule)
install(DIRECTORY include/ DESTINATION include)
install(TARGETS DeLI EXPORT DeLITargets)
install(EXPORT DeLITargets 
    FILE DeLITargets.cmake
    DESTINATION lib/cmake/DeLI
)

# Create config file for find_package
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeLIConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DeLIConfig.cmake"
    INSTALL_DESTINATION lib/cmake/DeLI
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DeLIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES 
    "${CMAKE_CURRENT_BINARY_DIR}/DeLIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DeLIConfigVersion.cmake"
    DESTINATION lib/cmake/DeLI
)
