#include <gtest/gtest.h>
#include "bio_engine.h"
#include <vector>

using namespace genesis;

class MemorySystemTest : public ::testing::Test {
protected:
    BioEngine engine;
    
    void SetUp() override {
        // Initialize basic structure
        engine.init_structured(100, 1000, 42); 
    }
};

// -----------------------------------------------------------------------------
// Test 1: Differential Plasticity (Fast vs Slow Learning)
// -----------------------------------------------------------------------------
TEST_F(MemorySystemTest, DifferentialPlasticity) {
    // This test verifies that the engine correctly creates layers with different
    // plasticity scales and that synapses can be created between them.
    
    // 1. Setup Architecture
    uint32_t input_start = engine.create_layer("Input", 1, LAYER_ID_INPUT);
    
    // "Slow" Cortex (Scale = 1.0)
    uint32_t slow_start = engine.create_layer("Cortex", 1, LAYER_ID_CORTEX, PLASTICITY_SCALE_CORTEX);
    
    // "Fast" Hippocampus (Scale = 10.0)
    uint32_t fast_start = engine.create_layer("Hippocampus", 1, LAYER_ID_HIPPOCAMPUS, PLASTICITY_SCALE_HIPPOCAMPUS);

    // Verify layers were created
    EXPECT_EQ(engine.get_layers().size(), 3);
    EXPECT_TRUE(engine.get_layers().count("Input") > 0);
    EXPECT_TRUE(engine.get_layers().count("Cortex") > 0);
    EXPECT_TRUE(engine.get_layers().count("Hippocampus") > 0);
    
    // Connect Input -> Slow
    size_t slow_conns = engine.connect_full("Input", "Cortex", 0.1f, 1);
    // Connect Input -> Fast
    size_t fast_conns = engine.connect_full("Input", "Hippocampus", 0.1f, 1);
    
    EXPECT_GT(slow_conns, 0) << "Should create connections to Cortex";
    EXPECT_GT(fast_conns, 0) << "Should create connections to Hippocampus";
    
    engine.bake_topology();

    // 2. Verify neurons have correct plasticity scale
    const auto& neurons = engine.get_neurons();
    EXPECT_FLOAT_EQ(neurons.plasticity_scale[slow_start], PLASTICITY_SCALE_CORTEX) 
        << "Cortex neuron should have plasticity scale of 1.0";
    EXPECT_FLOAT_EQ(neurons.plasticity_scale[fast_start], PLASTICITY_SCALE_HIPPOCAMPUS) 
        << "Hippocampus neuron should have plasticity scale of 10.0";
}

// -----------------------------------------------------------------------------
// Test 2: Pattern Completion (Basic Recurrent Connection)
// -----------------------------------------------------------------------------
TEST_F(MemorySystemTest, PatternCompletion) {
    // This test verifies that recurrent connections can be created and 
    // neurons fire when stimulated.
    
    // 1. Setup Recurrent Layer
    uint32_t mem_start = engine.create_layer("Memory", 2, LAYER_ID_HIPPOCAMPUS, 10.0f);
    
    // Verify layer was created
    EXPECT_EQ(engine.get_layers().size(), 1);
    EXPECT_TRUE(engine.get_layers().count("Memory") > 0);
    
    // Create recurrent connections
    size_t recurrent_conns = engine.connect_recurrent("Memory", 0.5f, 1, 1.0f);
    EXPECT_GT(recurrent_conns, 0) << "Should create recurrent connections";
    
    engine.bake_topology();

    // 2. Verify neurons can fire
    engine.set_dopamine(1.0f);
    
    // Stimulate first neuron
    engine.inject_stimulus(mem_start, 50.0f);
    engine.tick();
    
    const auto& neurons = engine.get_neurons();
    
    // First neuron should fire with strong stimulus
    EXPECT_TRUE(neurons.has_fired[mem_start]) 
        << "N0 should fire with strong stimulus";
    
    // Verify plasticity scale is set correctly
    EXPECT_FLOAT_EQ(neurons.plasticity_scale[mem_start], 10.0f)
        << "Memory neuron should have high plasticity scale";
}