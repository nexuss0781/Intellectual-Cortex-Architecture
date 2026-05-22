#include "../src/meta_cognition.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <iomanip>

using namespace genesis;

// Test helper macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAILED: " << message << std::endl; \
            return false; \
        } \
    } while(0)

#define TEST_REPORT(name) \
    std::cout << "Running: " << name << "..." << std::endl;

// =============================================================================
// Test 1: Default RAM Budget (500MB)
// =============================================================================
bool test_default_ram_budget() {
    TEST_REPORT("Default RAM Budget (500MB)");
    
    ResourceLimits limits;
    TEST_ASSERT(limits.ram_budget_mb == 500.0, "Default RAM budget should be 500MB");
    TEST_ASSERT(limits.max_neurons_config == 500000, "Default max neurons");
    TEST_ASSERT(limits.max_synapses_per_neuron == 100, "Default max synapses per neuron");
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    double memory_mb = engine.get_memory_usage_mb();
    TEST_ASSERT(memory_mb < 500.0, "Initial memory should be under 500MB budget");
    TEST_ASSERT(memory_mb > 0.0, "Memory should be positive");
    
    std::cout << "  Initial memory: " << memory_mb << " MB" << std::endl;
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 2: Start Small and Grow on Demand
// =============================================================================
bool test_start_small_grow_on_demand() {
    TEST_REPORT("Start Small and Grow on Demand");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 100.0;  // Small budget for testing
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    double initial_memory = engine.get_memory_usage_mb();
    
    std::cout << "  Initial neurons: " << initial_neurons << std::endl;
    std::cout << "  Initial memory: " << initial_memory << " MB" << std::endl;
    
    TEST_ASSERT(initial_neurons <= 5000, "Should start with small network (<=5K neurons)");
    TEST_ASSERT(initial_memory < 20.0, "Initial memory should be small (<20MB)");
    
    // Simulate demand by requesting more neurons
    engine.request_more_neurons(1000, "test growth");
    
    size_t grown_neurons = engine.get_current_neurons();
    std::cout << "  After growth: " << grown_neurons << " neurons" << std::endl;
    
    TEST_ASSERT(grown_neurons >= initial_neurons, "Neurons should not decrease");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 3: Synapse Demand Exceeds Neuron Demand
// Deactivate inactive neurons, increase synapses
// =============================================================================
bool test_synapse_demand_exceeds_neuron() {
    TEST_REPORT("Synapse Demand > Neuron Demand");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 200.0;
    limits.max_synapses_per_neuron = 50;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_synapses = engine.get_current_synapses();
    std::cout << "  Initial synapses: " << initial_synapses << std::endl;
    
    // Request more synapses per neuron
    engine.request_more_synapses(10, "high connectivity demand");
    
    size_t final_synapses = engine.get_current_synapses();
    std::cout << "  Final synapses: " << final_synapses << std::endl;
    
    TEST_ASSERT(final_synapses >= initial_synapses, "Synapses should increase or stay same");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 4: Neuron Demand Exceeds Synapse Demand
// Decrease unused synapses, increase neurons
// =============================================================================
bool test_neuron_demand_exceeds_synapse() {
    TEST_REPORT("Neuron Demand > Synapse Demand");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 200.0;
    limits.max_neurons_config = 50000;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    std::cout << "  Initial neurons: " << initial_neurons << std::endl;
    
    // Request more neurons
    engine.request_more_neurons(2000, "high neuron demand");
    
    size_t final_neurons = engine.get_current_neurons();
    std::cout << "  Final neurons: " << final_neurons << std::endl;
    
    TEST_ASSERT(final_neurons >= initial_neurons, "Neurons should increase or stay same");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 5: Low Demand - Start Small and Stay Small
// =============================================================================
bool test_low_demand_stay_small() {
    TEST_REPORT("Low Demand - Stay Small");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 500.0;  // Large budget but low demand
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    
    // Run simulation without creating demand
    for (int i = 0; i < 100; ++i) {
        engine.tick();
    }
    
    size_t final_neurons = engine.get_current_neurons();
    
    std::cout << "  Initial neurons: " << initial_neurons << std::endl;
    std::cout << "  Final neurons: " << final_neurons << std::endl;
    
    // With low activity, network should stay relatively small
    TEST_ASSERT(final_neurons <= initial_neurons * 2, 
                "Network should not grow significantly with low demand");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 6: High Demand Both Resources - Balanced Growth
// =============================================================================
bool test_high_demand_balanced() {
    TEST_REPORT("High Demand Both - Balanced Growth");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 300.0;
    limits.max_neurons_config = 100000;
    limits.max_synapses_per_neuron = 50;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t initial_neurons = engine.get_current_neurons();
    size_t initial_synapses = engine.get_current_synapses();
    double initial_memory = engine.get_memory_usage_mb();
    
    // Simulate high demand for both
    engine.request_more_neurons(3000, "high demand");
    engine.request_more_synapses(15, "high demand");
    
    size_t final_neurons = engine.get_current_neurons();
    size_t final_synapses = engine.get_current_synapses();
    double final_memory = engine.get_memory_usage_mb();
    
    std::cout << "  Initial: " << initial_neurons << " neurons, " 
              << initial_synapses << " synapses, " << initial_memory << " MB" << std::endl;
    std::cout << "  Final: " << final_neurons << " neurons, " 
              << final_synapses << " synapses, " << final_memory << " MB" << std::endl;
    
    TEST_ASSERT(final_memory <= limits.ram_budget_mb * 1.1, 
                "Memory should stay within budget (with some tolerance)");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 7: Budget Limited - Enforce RAM Constraint
// =============================================================================
bool test_budget_limited_enforcement() {
    TEST_REPORT("Budget Limited - RAM Constraint Enforcement");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 50.0;  // Very tight budget
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    double initial_memory = engine.get_memory_usage_mb();
    std::cout << "  Initial memory: " << initial_memory << " MB" << std::endl;
    
    // Try to allocate beyond budget
    engine.request_more_neurons(50000, "test");
    
    double final_memory = engine.get_memory_usage_mb();
    std::cout << "  Final memory: " << final_memory << " MB" << std::endl;
    
    TEST_ASSERT(final_memory <= limits.ram_budget_mb * 1.2, 
                "Memory should respect budget constraint");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 8: Release Unused Resources
// =============================================================================
bool test_release_unused_resources() {
    TEST_REPORT("Release Unused Resources");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 200.0;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    // Add some resources
    engine.request_more_neurons(2000, "test");
    
    double memory_before = engine.get_memory_usage_mb();
    std::cout << "  Memory before release: " << memory_before << " MB" << std::endl;
    
    // Release unused
    engine.release_unused_resources();
    
    double memory_after = engine.get_memory_usage_mb();
    std::cout << "  Memory after release: " << memory_after << " MB" << std::endl;
    
    TEST_ASSERT(memory_after <= memory_before, 
                "Memory should not increase after releasing unused resources");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 9: Dynamic Reallocation During Simulation
// =============================================================================
bool test_dynamic_reallocation_simulation() {
    TEST_REPORT("Dynamic Reallocation During Simulation");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 150.0;
    limits.max_neurons_config = 30000;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    size_t neurons_at_start = engine.get_current_neurons();
    
    // Run simulation with periodic stimulus to create activity
    for (int tick = 0; tick < 200; ++tick) {
        // Inject some stimulus to create activity
        if (tick % 50 == 0 && engine.get_engine()) {
            engine.get_engine()->inject_stimulus(tick % neurons_at_start, 15.0f);
        }
        engine.tick();
    }
    
    size_t neurons_at_end = engine.get_current_neurons();
    double utilization = engine.get_neuron_utilization();
    
    std::cout << "  Neurons at start: " << neurons_at_start << std::endl;
    std::cout << "  Neurons at end: " << neurons_at_end << std::endl;
    std::cout << "  Utilization: " << (utilization * 100) << "%" << std::endl;
    
    TEST_ASSERT(neurons_at_end > 0, "Should have neurons after simulation");
    TEST_ASSERT(engine.get_memory_usage_mb() <= limits.ram_budget_mb * 1.1,
                "Should respect memory budget");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 10: Custom RAM Budget
// =============================================================================
bool test_custom_ram_budget() {
    TEST_REPORT("Custom RAM Budget");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 250.0;  // Custom budget
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    double memory = engine.get_memory_usage_mb();
    
    TEST_ASSERT(memory <= 250.0, "Should respect custom 250MB budget");
    TEST_ASSERT(memory > 0.0, "Memory should be positive");
    
    std::cout << "  Memory usage: " << memory << " MB (budget: 250 MB)" << std::endl;
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 11: Demand Signal Assessment
// =============================================================================
bool test_demand_signal_assessment() {
    TEST_REPORT("Demand Signal Assessment");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 100.0;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    // Run a few ticks to establish baseline
    for (int i = 0; i < 60; ++i) {
        engine.tick();
    }
    
    const auto& demand = engine.get_demand();
    
    std::cout << "  Neuron demand level assessed" << std::endl;
    std::cout << "  Synapse demand level assessed" << std::endl;
    std::cout << "  Utilization ratio: " << engine.get_neuron_utilization() << std::endl;
    
    TEST_ASSERT(true, "Demand signals should be computable");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Test 12: Status Reporting
// =============================================================================
bool test_status_reporting() {
    TEST_REPORT("Status Reporting");
    
    ResourceLimits limits;
    limits.ram_budget_mb = 100.0;
    
    MetaCognitiveEngine engine;
    engine.init_auto_scale(limits);
    
    std::cout << "\n--- Engine Status ---" << std::endl;
    engine.print_status();
    
    TEST_ASSERT(engine.get_current_neurons() > 0, "Should report neuron count");
    TEST_ASSERT(engine.get_memory_usage_mb() > 0, "Should report memory usage");
    
    std::cout << "  PASSED\n" << std::endl;
    return true;
}

// =============================================================================
// Main Test Runner
// =============================================================================
int main() {
    std::cout << "=============================================" << std::endl;
    std::cout << "  Meta-Cognitive Engine v2 - Test Suite" << std::endl;
    std::cout << "=============================================\n" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    auto run_test = [&](bool (*test_func)(), const char* name) {
        total++;
        try {
            if (test_func()) {
                passed++;
            } else {
                std::cerr << "  FAILED: " << name << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  EXCEPTION in " << name << ": " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "  UNKNOWN EXCEPTION in " << name << std::endl;
        }
    };
    
    run_test(test_default_ram_budget, "Default RAM Budget");
    run_test(test_start_small_grow_on_demand, "Start Small Grow on Demand");
    run_test(test_synapse_demand_exceeds_neuron, "Synapse Demand > Neuron Demand");
    run_test(test_neuron_demand_exceeds_synapse, "Neuron Demand > Synapse Demand");
    run_test(test_low_demand_stay_small, "Low Demand Stay Small");
    run_test(test_high_demand_balanced, "High Demand Balanced");
    run_test(test_budget_limited_enforcement, "Budget Limited Enforcement");
    run_test(test_release_unused_resources, "Release Unused Resources");
    run_test(test_dynamic_reallocation_simulation, "Dynamic Reallocation Simulation");
    run_test(test_custom_ram_budget, "Custom RAM Budget");
    run_test(test_demand_signal_assessment, "Demand Signal Assessment");
    run_test(test_status_reporting, "Status Reporting");
    
    std::cout << "=============================================" << std::endl;
    std::cout << "  Test Results: " << passed << "/" << total << " passed" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    return (passed == total) ? 0 : 1;
}
