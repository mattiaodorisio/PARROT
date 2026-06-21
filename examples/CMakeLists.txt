# Examples CMakeLists.txt

# Example 1
add_executable(example1 example1.cpp)
target_link_libraries(example1 PARROT)

# Set output directory for examples
set_target_properties(example1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
)
