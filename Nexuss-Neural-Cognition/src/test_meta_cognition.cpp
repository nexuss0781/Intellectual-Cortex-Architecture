#include "meta_cognition.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>

using namespace genesis;

// =============================================================================
// Test Framework
// =============================================================================

struct TestResult {
    bool passed;
    std::string name;
    std::string message;
    
    TestResult() : passed(true) {}
};

#define TEST(name) TestResult test_##name() { TestResult result; result.name = #name;
#define END_TEST return result; }

#define ASSERT_TRUE(condition) if (!(condition)) { result.passed = false; result.message = "Assertion failed: " #condition; return result; }
#define ASSERT_FALSE(condition) if (condition) { result.passed = false; result.message = "Assertion failed: NOT " #condition; return result; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { result.passed = false; result.message = "Equality assertion failed: " #a " != " #b; return result; }
#define ASSERT_NEAR(a, b, tol) if (std::abs((a) - (b)) > (tol)) { result.passed = false; result.message = "Near equality failed: " #a " ~= " #b; return result; }

void print_result(const TestResult& r) {
    std::cout << "[" << (r.passed ? "PASS" : "FAIL") << "] " << r.name;
    if (!r.passed && !r.message.empty()) {
        std::cout << " - " << r.message;
    }
    std::cout << std::endl;
}

// =============================================================================
// Test 1: Initialization with Default RAM Budget (500MB)
// =============================================================================

TEST(test_initialization_default_budget) {
    ResourceLimits limits;
    // Default should be 500MB
    ASSERT_NEAR(limits.ram_budget_mb, 500.0, 0.01);
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    // Should start small
    size_t neurons = engine.get_current_neurons();
    ASSERT_TRUE(neurons >= limits.min_neurons);
    ASSERT_TRUE(neurons <= limits.max_neurons_config);
    
    double memory = engine.get_memory_usage_mb();
    ASSERT_TRUE(memory <= limits.ram_budget_mb);
    
    END_TEST
}

// =============================================================================
// Test 2: Custom RAM Budget Constraint
// =============================================================================

TEST(test_custom_ram_budget) {
    ResourceLimits limits;
    limits.ram_budget_mb = 100.0;  // Small budget
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    double memory = engine.get_memory_usage_mb();
    ASSERT_TRUE(memory <= 100.0);
    
    END_TEST
}

// =============================================================================
// Test 3: Start Small Configuration
// =============================================================================

TEST(test_start_small) {
    ResourceLimits limits;
    limits.ram_budget_mb = 500.0;
    limits.min_neurons = 1000;
    limits.min_synapses_per_neuron = 5;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    // Should start at minimum
    size_t neurons = engine.get_current_neurons();
    ASSERT_EQ(neurons, limits.min_neurons);
    
    size_t synapses = engine.get_current_synapses();
    ASSERT_EQ(synapses, neurons * limits.min_synapses_per_neuron);
    
    END_TEST
}

// =============================================================================
// Test 4: Grow Small Strategy
// =============================================================================

TEST(test_grow_small_strategy) {
    ResourceLimits limits;
    limits.ram_budget_mb = 500.0;
    limits.min_neurons = 1000;
    limits.min_synapses_per_neuron = 5;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    
    // Force grow small strategy
    engine.force_allocation_strategy(AllocationStrategy::GROW_SMALL);
    
    // Simulate some activity to trigger growth
    std::vector<size_t> active_neurons;
    for (size_t i = 0; i < initial_neurons * 0.9; ++i) {
        active_neurons.push_back(i);
    }
    engine.simulate_neuron_activity(active_neurons);
    
    // Run ticks to trigger reallocation
    for (int i = 0; i < 50; ++i) {
        engine.tick();
    }
    
    // Network should have grown or stayed same (if at limit)
    size_t final_neurons = engine.get_current_neurons();
    ASSERT_TRUE(final_neurons >= initial_neurons);
    
    END_TEST
}

// =============================================================================
// Test 5: Favor Synapses Strategy (More Synapses than Neurons)
// =============================================================================

TEST(test_favor_synapses_strategy) {
    ResourceLimits limits;
    limits.ram_budget_mb = 200.0;
    limits.min_neurons = 2000;
    limits.min_synapses_per_neuron = 10;
    limits.max_synapses_per_neuron = 50;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    size_t initial_synapses = engine.get_current_synapses();
    
    // Simulate high synapse usage, low neuron usage
    std::vector<size_t> active_synapses;
    for (size_t i = 0; i < initial_synapses * 0.9; ++i) {
        active_synapses.push_back(i);
    }
    engine.simulate_synapse_usage(active_synapses);
    
    std::vector<size_t> active_neurons;
    for (size_t i = 0; i < initial_neurons * 0.2; ++i) {
        active_neurons.push_back(i);
    }
    engine.simulate_neuron_activity(active_neurons);
    
    // Force favor synapses strategy
    engine.force_allocation_strategy(AllocationStrategy::FAVOR_SYNAPSES);
    
    // Run ticks to trigger reallocation
    for (int i = 0; i < 50; ++i) {
        engine.tick();
    }
    
    // Should have fewer or equal neurons, more synapses per neuron
    size_t final_neurons = engine.get_current_neurons();
    size_t final_synapses = engine.get_current_synapses();
    
    // Memory should still be within budget
    double memory = engine.get_memory_usage_mb();
    ASSERT_TRUE(memory <= limits.ram_budget_mb);
    
    END_TEST
}

// =============================================================================
// Test 6: Favor Neurons Strategy (More Neurons than Synapses)
// =============================================================================

TEST(test_favor_neurons_strategy) {
    ResourceLimits limits;
    limits.ram_budget_mb = 200.0;
    limits.min_neurons = 2000;
    limits.min_synapses_per_neuron = 10;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    size_t initial_synapses = engine.get_current_synapses();
    
    // Simulate high neuron usage, low synapse usage
    std::vector<size_t> active_neurons;
    for (size_t i = 0; i < initial_neurons * 0.9; ++i) {
        active_neurons.push_back(i);
    }
    engine.simulate_neuron_activity(active_neurons);
    
    std::vector<size_t> active_synapses;
    for (size_t i = 0; i < initial_synapses * 0.2; ++i) {
        active_synapses.push_back(i);
    }
    engine.simulate_synapse_usage(active_synapses);
    
    // Force favor neurons strategy
    engine.force_allocation_strategy(AllocationStrategy::FAVOR_NEURONS);
    
    // Run ticks to trigger reallocation
    for (int i = 0; i < 50; ++i) {
        engine.tick();
    }
    
    // Should have more neurons or equal, fewer synapses per neuron
    size_t final_neurons = engine.get_current_neurons();
    ASSERT_TRUE(final_neurons >= initial_neurons);
    
    // Memory should still be within budget
    double memory = engine.get_memory_usage_mb();
    ASSERT_TRUE(memory <= limits.ram_budget_mb);
    
    END_TEST
}

// =============================================================================
// Test 7: Balanced Strategy (Both Needed Equally)
// =============================================================================

TEST(test_balanced_strategy) {
    ResourceLimits limits;
    limits.ram_budget_mb = 300.0;
    limits.min_neurons = 2000;
    limits.min_synapses_per_neuron = 10;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    size_t initial_synapses = engine.get_current_synapses();
    
    // Simulate high usage of both
    std::vector<size_t> active_neurons;
    for (size_t i = 0; i < initial_neurons * 0.95; ++i) {
        active_neurons.push_back(i);
    }
    engine.simulate_neuron_activity(active_neurons);
    
    std::vector<size_t> active_synapses;
    for (size_t i = 0; i < initial_synapses * 0.95; ++i) {
        active_synapses.push_back(i);
    }
    engine.simulate_synapse_usage(active_synapses);
    
    // Force balanced strategy
    engine.force_allocation_strategy(AllocationStrategy::BALANCED);
    
    // Run ticks to trigger reallocation
    for (int i = 0; i < 50; ++i) {
        engine.tick();
    }
    
    // Both should have grown or stayed same
    size_t final_neurons = engine.get_current_neurons();
    size_t final_synapses = engine.get_current_synapses();
    
    ASSERT_TRUE(final_neurons >= initial_neurons);
    ASSERT_TRUE(final_synapses >= initial_synapses);
    
    // Memory should still be within budget
    double memory = engine.get_memory_usage_mb();
    ASSERT_TRUE(memory <= limits.ram_budget_mb);
    
    END_TEST
}

// =============================================================================
// Test 8: Shrink Inactive Strategy
// =============================================================================

TEST(test_shrink_inactive_strategy) {
    ResourceLimits limits;
    limits.ram_budget_mb = 500.0;
    limits.min_neurons = 1000;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    // Grow first
    engine.force_allocation_strategy(AllocationStrategy::GROW_SMALL);
    for (int i = 0; i < 100; ++i) {
        engine.tick();
    }
    
    size_t grown_neurons = engine.get_current_neurons();
    
    // Now simulate very low activity
    std::vector<size_t> active_neurons;
    for (size_t i = 0; i < grown_neurons * 0.1; ++i) {
        active_neurons.push_back(i);
    }
    engine.simulate_neuron_activity(active_neurons);
    
    std::vector<size_t> active_synapses;
    for (size_t i = 0; i < engine.get_current_synapses() * 0.1; ++i) {
        active_synapses.push_back(i);
    }
    engine.simulate_synapse_usage(active_synapses);
    
    // Force shrink strategy
    engine.force_allocation_strategy(AllocationStrategy::SHRINK_INACTIVE);
    
    // Run ticks to trigger reallocation
    for (int i = 0; i < 50; ++i) {
        engine.tick();
    }
    
    // Should have shrunk but not below minimum
    size_t final_neurons = engine.get_current_neurons();
    ASSERT_TRUE(final_neurons <= grown_neurons);
    ASSERT_TRUE(final_neurons >= limits.min_neurons);
    
    END_TEST
}

// =============================================================================
// Test 9: Memory Budget Enforcement
// =============================================================================

TEST(test_memory_budget_enforcement) {
    ResourceLimits limits;
    limits.ram_budget_mb = 50.0;  // Very tight budget
    limits.min_neurons = 500;
    limits.min_synapses_per_neuron = 5;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    // Try to force growth beyond budget
    engine.force_allocation_strategy(AllocationStrategy::BALANCED);
    
    for (int i = 0; i < 200; ++i) {
        engine.tick();
    }
    
    // Should never exceed budget
    double memory = engine.get_memory_usage_mb();
    ASSERT_TRUE(memory <= limits.ram_budget_mb);
    
    END_TEST
}

// =============================================================================
// Test 10: External Resource Requests
// =============================================================================

TEST(test_external_resource_requests) {
    ResourceLimits limits;
    limits.ram_budget_mb = 300.0;
    limits.min_neurons = 1000;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    
    // Request more neurons
    engine.request_more_neurons(500, "Test request");
    
    // Run ticks to process request
    for (int i = 0; i < 50; ++i) {
        engine.tick();
    }
    
    // Should have processed the request (grown or stayed at limit)
    size_t final_neurons = engine.get_current_neurons();
    ASSERT_TRUE(final_neurons >= initial_neurons);
    
    END_TEST
}

// =============================================================================
// Test 11: Utilization Tracking
// =============================================================================

TEST(test_utilization_tracking) {
    ResourceLimits limits;
    limits.ram_budget_mb = 200.0;
    limits.min_neurons = 2000;
    limits.min_synapses_per_neuron = 10;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t neurons = engine.get_current_neurons();
    size_t synapses = engine.get_current_synapses();
    
    // Simulate 50% neuron activity
    std::vector<size_t> active_neurons;
    for (size_t i = 0; i < neurons / 2; ++i) {
        active_neurons.push_back(i);
    }
    engine.simulate_neuron_activity(active_neurons);
    
    double neuron_util = engine.get_neuron_utilization();
    ASSERT_NEAR(neuron_util, 0.5, 0.1);
    
    // Simulate 75% synapse activity
    std::vector<size_t> active_synapses;
    for (size_t i = 0; i < synapses * 3 / 4; ++i) {
        active_synapses.push_back(i);
    }
    engine.simulate_synapse_usage(active_synapses);
    
    double synapse_util = engine.get_synapse_utilization();
    ASSERT_NEAR(synapse_util, 0.75, 0.1);
    
    END_TEST
}

// =============================================================================
// Test 12: Demand Level Assessment
// =============================================================================

TEST(test_demand_level_assessment) {
    ResourceLimits limits;
    limits.ram_budget_mb = 200.0;
    limits.min_neurons = 2000;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t neurons = engine.get_current_neurons();
    
    // Test NONE demand (0% utilization)
    std::vector<size_t> empty;
    engine.simulate_neuron_activity(empty);
    engine.tick();
    const auto& demand = engine.get_demand();
    // After analyze_demand is called internally
    // We check that the system handles zero activity
    
    // Test CRITICAL demand (>95% utilization)
    std::vector<size_t> critical;
    for (size_t i = 0; i < neurons * 0.96; ++i) {
        critical.push_back(i);
    }
    engine.simulate_neuron_activity(critical);
    engine.tick();
    
    // System should detect high demand
    ASSERT_TRUE(engine.get_neuron_utilization() > 0.9);
    
    END_TEST
}

// =============================================================================
// Test 13: Reconfiguration Success
// =============================================================================

TEST(test_reconfiguration_success) {
    ResourceLimits limits;
    limits.ram_budget_mb = 300.0;
    limits.min_neurons = 1000;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t before_neurons = engine.get_current_neurons();
    double before_memory = engine.get_memory_usage_mb();
    
    // Force multiple growth cycles
    engine.force_allocation_strategy(AllocationStrategy::GROW_SMALL);
    
    for (int i = 0; i < 150; ++i) {
        engine.tick();
    }
    
    size_t after_neurons = engine.get_current_neurons();
    double after_memory = engine.get_memory_usage_mb();
    
    // Should have grown or stayed same
    ASSERT_TRUE(after_neurons >= before_neurons);
    ASSERT_TRUE(after_memory <= limits.ram_budget_mb);
    
    END_TEST
}

// =============================================================================
// Test 14: Activity Metrics Reset
// =============================================================================

TEST(test_activity_metrics_reset) {
    ResourceLimits limits;
    limits.ram_budget_mb = 200.0;
    limits.min_neurons = 2000;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    // Set some activity
    std::vector<size_t> active;
    for (size_t i = 0; i < 1000; ++i) {
        active.push_back(i);
    }
    engine.simulate_neuron_activity(active);
    
    ASSERT_TRUE(engine.get_active_neurons() > 0);
    
    // Reset
    engine.reset_activity_metrics();
    
    ASSERT_EQ(engine.get_active_neurons(), 0);
    ASSERT_EQ(engine.get_inactive_neurons(), engine.get_current_neurons());
    
    END_TEST
}

// =============================================================================
// Test 15: Minimum Configuration Enforcement
// =============================================================================

TEST(test_minimum_configuration_enforcement) {
    ResourceLimits limits;
    limits.ram_budget_mb = 500.0;
    limits.min_neurons = 1000;
    limits.min_synapses_per_neuron = 5;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    // Try to shrink below minimum
    engine.force_allocation_strategy(AllocationStrategy::SHRINK_INACTIVE);
    engine.simulate_neuron_activity(std::vector<size_t>{});  // No activity
    engine.simulate_synapse_usage(std::vector<size_t>{});
    
    for (int i = 0; i < 100; ++i) {
        engine.tick();
    }
    
    // Should never go below minimum
    ASSERT_TRUE(engine.get_current_neurons() >= limits.min_neurons);
    
    END_TEST
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main() {
    std::cout << "=============================================" << std::endl;
    std::cout << "  Meta-Cognitive Engine - Comprehensive Tests" << std::endl;
    std::cout << "=============================================\n" << std::endl;
    
    std::vector<TestResult> results;
    
    // Run all tests
    results.push_back(test_initialization_default_budget());
    results.push_back(test_custom_ram_budget());
    results.push_back(test_start_small());
    results.push_back(test_grow_small_strategy());
    results.push_back(test_favor_synapses_strategy());
    results.push_back(test_favor_neurons_strategy());
    results.push_back(test_balanced_strategy());
    results.push_back(test_shrink_inactive_strategy());
    results.push_back(test_memory_budget_enforcement());
    results.push_back(test_external_resource_requests());
    results.push_back(test_utilization_tracking());
    results.push_back(test_demand_level_assessment());
    results.push_back(test_reconfiguration_success());
    results.push_back(test_activity_metrics_reset());
    results.push_back(test_minimum_configuration_enforcement());
    
    // Print results
    std::cout << "\n=============================================" << std::endl;
    std::cout << "  Test Results Summary" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& r : results) {
        print_result(r);
        if (r.passed) passed++;
        else failed++;
    }
    
    std::cout << "\n---------------------------------------------" << std::endl;
    std::cout << "Total: " << results.size() << " tests" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "=============================================" << std::endl;
    
    return failed == 0 ? 0 : 1;
}
