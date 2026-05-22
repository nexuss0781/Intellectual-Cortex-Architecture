cmake_minimum_required(VERSION 3.14)
project(nexuss_neural_cognition CXX)

# Enforce C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization Flags
if(MSVC)
    add_compile_options(/O2 /arch:AVX2 /W4)
else()
    add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic)
endif()

# Core Logic Library
add_library(bio_core
    src/bio_engine.cpp
    src/meta_cognition.cpp
)
target_include_directories(bio_core PUBLIC src)

# Main Executable
add_executable(nexuss_sim src/main.cpp)
target_link_libraries(nexuss_sim PRIVATE bio_core)

# ------------------------------------------------------------------------------
# Testing Infrastructure
# ------------------------------------------------------------------------------
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test 1: Physics (Phase I)
add_executable(physics_tests tests/physics_tests.cpp)
target_link_libraries(physics_tests PRIVATE bio_core GTest::gtest_main)

# Test 2: Sensory (Phase II)
add_executable(sensory_tests tests/sensory_tests.cpp)
target_link_libraries(sensory_tests PRIVATE bio_core GTest::gtest_main)

# Test 3: Memory (Phase III - NEW)
add_executable(memory_tests tests/memory_tests.cpp)
target_link_libraries(memory_tests PRIVATE bio_core GTest::gtest_main)

# Integration
include(GoogleTest)
gtest_discover_tests(physics_tests)
gtest_discover_tests(sensory_tests)
gtest_discover_tests(memory_tests)

# Test 4: Meta-Cognition Auto-Scaling
add_executable(meta_cognition_demo src/demo_meta_cognition.cpp)
target_link_libraries(meta_cognition_demo PRIVATE bio_core)

# Test 5: Meta-Cognition v2 (Dynamic RAM-based allocation)
add_executable(test_meta_cognition_v2 tests/test_meta_cognition_v2.cpp)
target_link_libraries(test_meta_cognition_v2 PRIVATE bio_core)

# Test 6: UIN - Universal Intellectual Neuron (Phase IV+)
add_executable(uin_tests tests/uin_tests.cpp src/intellectual/uin_engine.cpp)
target_link_libraries(uin_tests PRIVATE bio_core GTest::gtest_main)
target_include_directories(uin_tests PRIVATE src)

# Test 7: UIN Full-Scale Stress Test (270K neurons, 13.5M synapses)
add_executable(uin_stress_test tests/uin_stress_test.cpp src/intellectual/uin_engine.cpp)
target_link_libraries(uin_stress_test PRIVATE bio_core GTest::gtest_main)
target_include_directories(uin_stress_test PRIVATE src)
