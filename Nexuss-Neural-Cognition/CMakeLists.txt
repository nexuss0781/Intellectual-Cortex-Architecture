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

# Stage 0: deterministic substrate stabilization and transition harness
add_executable(stage0_harness tests/stage0_harness.cpp src/intellectual/uin_engine.cpp)
target_link_libraries(stage0_harness PRIVATE bio_core)
target_include_directories(stage0_harness PRIVATE src)

# Register all non-GTest validation executables with CTest as integration tests.
add_test(NAME meta_cognition_v2 COMMAND test_meta_cognition_v2)
add_test(NAME uin_tests COMMAND uin_tests)
add_test(NAME uin_stress_test COMMAND uin_stress_test)
add_test(NAME stage0_harness
         COMMAND stage0_harness --seed 424242 --artifact-dir ${CMAKE_BINARY_DIR}/artifacts/stage-0)

# Stage 1: modular local-learning and continual-stream harness
add_executable(stage1_harness tests/stage1_harness.cpp)
target_link_libraries(stage1_harness PRIVATE bio_core)
target_include_directories(stage1_harness PRIVATE src)
add_test(NAME stage1_harness
         COMMAND stage1_harness --seed 424242 --artifact-dir ${CMAKE_BINARY_DIR}/artifacts/stage-1)

# Stage 2: persistent memory, replay, consolidation, and transactional IDs
add_executable(stage2_harness tests/stage2_harness.cpp src/learning/memory_system.cpp)
target_link_libraries(stage2_harness PRIVATE bio_core)
target_include_directories(stage2_harness PRIVATE src)
add_test(NAME stage2_harness
         COMMAND stage2_harness --seed 424242 --artifact-dir ${CMAKE_BINARY_DIR}/artifacts/stage-2)

# Stage 3: predictive processing, precision attention, and global workspace
add_executable(stage3_harness tests/stage3_harness.cpp)
target_link_libraries(stage3_harness PRIVATE bio_core)
target_include_directories(stage3_harness PRIVATE src)
add_test(NAME stage3_harness
         COMMAND stage3_harness --seed 424242 --artifact-dir ${CMAKE_BINARY_DIR}/artifacts/stage-3)

# Stage 4: grounded language acquisition, semantic pointers, and constructions
add_executable(stage4_harness tests/stage4_harness.cpp)
target_link_libraries(stage4_harness PRIVATE bio_core)
target_include_directories(stage4_harness PRIVATE src)
add_test(NAME stage4_harness
         COMMAND stage4_harness --seed 424242 --artifact-dir ${CMAKE_BINARY_DIR}/artifacts/stage-4)

# Stage 5: reasoning, causal intervention, executive control, and metacognition
add_executable(stage5_harness tests/stage5_harness.cpp)
target_link_libraries(stage5_harness PRIVATE bio_core)
target_include_directories(stage5_harness PRIVATE src)
add_test(NAME stage5_harness
         COMMAND stage5_harness --seed 424242 --artifact-dir ${CMAKE_BINARY_DIR}/artifacts/stage-5)

# Stage 6: grounding, deterministic multimodal development, safety, and transfer
add_executable(stage6_harness tests/stage6_harness.cpp)
target_link_libraries(stage6_harness PRIVATE bio_core)
target_include_directories(stage6_harness PRIVATE src)
add_test(NAME stage6_harness
         COMMAND stage6_harness --seed 424242 --artifact-dir ${CMAKE_BINARY_DIR}/artifacts/stage-6)
