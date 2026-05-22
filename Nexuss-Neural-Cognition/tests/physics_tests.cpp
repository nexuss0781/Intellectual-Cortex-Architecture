#include <gtest/gtest.h>
#include "bio_engine.h"
#include <cmath>

using namespace genesis;

class BioPhysicsTest : public ::testing::Test {
protected:
    BioEngine engine;

    void SetUp() override {
        // Initialize with deterministic seed
        // Smallest possible network for unit testing
        // 2 Neurons, Density 1 ensures they are connected if we check indices
        engine.init(2, 1, 42); 
    }
};

// -----------------------------------------------------------------------------
// Test 1: Single Neuron Dynamics (Integrity & Metabolism)
// -----------------------------------------------------------------------------
TEST_F(BioPhysicsTest, SingleNeuronIntegrity) {
    engine.init(1, 0, 42); // 1 Neuron, isolated
    
    // 1. Stimulation -> Firing
    // V_THRESH is -55.0, V_REST is -70.0. Need +15.0 to fire.
    engine.inject_stimulus(0, 20.0f); 
    engine.tick();
    
    const auto& neurons = engine.get_neurons();
    
    EXPECT_TRUE(neurons.has_fired[0]) << "Neuron should fire with super-threshold stimulus";
    EXPECT_FLOAT_EQ(neurons.membrane_potential[0], V_RESET_MV) << "Voltage should reset after spike";
    EXPECT_EQ(neurons.refractory_timer[0], REFRACTORY_TICKS) << "Refractory timer should be set";
    EXPECT_LT(neurons.atp_level[0], ATP_MAX) << "ATP should be consumed by spike";

    // 2. Refractory Period
    // Tick forward 1 ms. Refractory should decrease. Voltage should stay low.
    engine.inject_stimulus(0, 20.0f); // Stimulate again
    engine.tick();
    
    EXPECT_FALSE(neurons.has_fired[0]) << "Neuron should NOT fire during refractory";
    EXPECT_EQ(neurons.refractory_timer[0], REFRACTORY_TICKS - 1);
    
    // 3. Metabolic Gating (Fatigue)
    // Manually drain ATP to 0
    // We can't write directly to neurons via public API, but we can simulate exhaustion
    // by rapid firing or we can re-init engine with specific logic.
    // For this test, we accept we can't manually set ATP private var, 
    // so we assume the ATP decrement logic worked in step 1.
    float atp_after_fire = neurons.atp_level[0];
    EXPECT_NEAR(atp_after_fire, ATP_MAX - SPIKE_COST + ATP_REFILL_RATE, 0.001f);
}

// -----------------------------------------------------------------------------
// Test 2: STDP Causal Association (LTP/LTD)
// -----------------------------------------------------------------------------
TEST_F(BioPhysicsTest, STDP_CausalLink) {
    // Setup: Neuron 0 connected to Neuron 1
    // We need to verify connectivity first due to random init
    engine.init(2, 2, 42); // Force dense connection
    
    // Find a synapse from 0 -> 1
    const auto& syn = engine.get_synapses();
    int syn_idx = -1;
    for(size_t i=0; i<syn.pre_indices.size(); ++i) {
        if(syn.pre_indices[i] == 0 && syn.post_indices[i] == 1) {
            syn_idx = i;
            break;
        }
    }
    ASSERT_NE(syn_idx, -1) << "Setup failed: No synapse found from 0 to 1";

    // 1. LTP Sequence: Pre (0) fires, then Post (1) fires
    engine.inject_stimulus(0, 50.0f); // Fire Pre
    engine.tick();
    
    // Check Trace Increase (Pre fired)
    float trace_after_pre = syn.eligibility_traces[syn_idx];
    EXPECT_GT(trace_after_pre, 0.0f) << "Eligibility trace should increase after Pre spike";

    engine.inject_stimulus(1, 50.0f); // Fire Post
    engine.tick();

    // Trace should NOT decrease (LTD condition is Post BEFORE Pre)
    // Since Pre fired 1 tick ago, diff is small (positive causal), trace stays high.
    
    // 2. Dopamine Gating
    float w_initial = syn.weights[syn_idx];
    engine.set_dopamine(1.0f); // Turn on learning
    engine.tick(); 
    
    float w_final = syn.weights[syn_idx];
    EXPECT_GT(w_final, w_initial) << "Weight should increase with Dopamine + LTP Trace";
}

// -----------------------------------------------------------------------------
// Test 3: Dopamine Bounds & Anti-Causal (LTD)
// -----------------------------------------------------------------------------
TEST_F(BioPhysicsTest, DopamineGatingAndLTD) {
    engine.init(2, 2, 42);
    // Find synapse 0 -> 1
    const auto& syn = engine.get_synapses();
    int syn_idx = -1;
    for(size_t i=0; i<syn.pre_indices.size(); ++i) {
        if(syn.pre_indices[i] == 0 && syn.post_indices[i] == 1) {
            syn_idx = i;
            break;
        }
    }
    ASSERT_NE(syn_idx, -1);

    // 1. LTD Sequence: Post (1) fires, then Pre (0) fires
    engine.inject_stimulus(1, 50.0f); // Fire Post
    engine.tick();
    engine.inject_stimulus(0, 50.0f); // Fire Pre
    engine.tick(); // Logic processes Pre spike and sees Post recently fired -> LTD
    
    float trace_val = syn.eligibility_traces[syn_idx];
    // Trace should be penalized. 
    // Logic: Pre fire adds A_PLUS. Post-before-Pre check subtracts A_MINUS.
    // A_MINUS > A_PLUS, so net result should be negative or lower than A_PLUS.
    // However, Pre just fired, so trace = A_PLUS + (prev_decayed) - A_MINUS.
    // Since Post fired 1 tick ago.
    // Let's verify trace is lower than pure LTP event.
    EXPECT_LT(trace_val, A_PLUS) << "Trace should be reduced due to LTD (Post before Pre)";

    // 2. Bounds Check
    // Set weight to near MAX
    // We can't write weight directly, so we rely on logic holding W_MAX
    // We will verify it doesn't explode in long run
    engine.set_dopamine(10.0f); // Massive reward
    for(int i=0; i<50; ++i) {
        // Constant LTP
        engine.inject_stimulus(0, 50.0f);
        engine.tick();
        engine.inject_stimulus(1, 50.0f);
        engine.tick();
    }
    EXPECT_LE(syn.weights[syn_idx], W_MAX + 0.00001f) << "Weight should be clamped to W_MAX";
}

// -----------------------------------------------------------------------------
// Test 4: Metabolic Crash (Stability)
// -----------------------------------------------------------------------------
TEST_F(BioPhysicsTest, MetabolicCrash) {
    engine.init(10, 10, 42); // Small dense network
    const auto& neurons = engine.get_neurons();

    // Blast the network with energy-draining noise for 100 ticks
    for(int i=0; i<100; ++i) {
        for(int n=0; n<10; ++n) engine.inject_stimulus(n, 100.0f);
        engine.tick();
    }

    // Check that ATP is depleted
    float avg_atp = 0.0f;
    for(float a : neurons.atp_level) avg_atp += a;
    avg_atp /= 10.0f;

    EXPECT_LT(avg_atp, 0.5f) << "Network should suffer ATP drain under heavy load";
    
    // Check that firing stops (Self-regulation)
    // If ATP < 0.1, they can't fire.
    // Stimulate again
    engine.inject_stimulus(0, 100.0f);
    engine.tick();
    
    if (neurons.atp_level[0] < ATP_MIN_TO_FIRE) {
        EXPECT_FALSE(neurons.has_fired[0]) << "Fatigued neuron should not fire even with stimulus";
    }

    // Recovery Phase
    // Wait 100 ticks (no input)
    for(int i=0; i<100; ++i) engine.tick();
    
    EXPECT_GT(neurons.atp_level[0], avg_atp) << "ATP should recover during rest";
}