#include "bio_engine.h"
#include "sensory/input_layer.h"
#include "sensory/cortex_layer.h"
#include "sensory/thalamus.h"
#include "sensory/spatial_utils.h"
#include "learning/hippocampus.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>

using namespace genesis;

// -----------------------------------------------------------------------------
// Phase III Experiment: "The Pavlovian Android"
// -----------------------------------------------------------------------------
// Objective: Demonstrate Associative Memory.
// 1. [Learning]: Pair a Visual Stimulus ("Bell") with an Auditory Stimulus ("Food").
// 2. [Recall]: Present ONLY Visual ("Bell"). Check if Auditory Cortex ("Food") fires.
// 3. [Sleep]: Enter Sleep mode and observe spontaneous replay.

// Configuration
constexpr size_t LAYER_DIM = 20; // Smaller layers for clear causality
constexpr size_t LAYER_SIZE = LAYER_DIM * LAYER_DIM; // 400 neurons per region

// We need: VisInput, AudInput, VisCortex, AudCortex, Hippocampus
constexpr size_t TOTAL_NEURONS = (LAYER_SIZE * 5); 
constexpr size_t MAX_SYNAPSES_EST = TOTAL_NEURONS * 200;

constexpr uint64_t RNG_SEED = 42;
constexpr int SIM_TICKS = 6000;
constexpr int LOG_INTERVAL = 100;

// Stimulus: A vertical line (Visual "Bell")
std::vector<float> create_visual_stimulus() {
    std::vector<float> data(LAYER_SIZE, 0.0f);
    int center_x = LAYER_DIM / 2;
    for (size_t y = 2; y < LAYER_DIM - 2; ++y) {
        data[y * LAYER_DIM + center_x] = 1.0f;
    }
    return data;
}

// Stimulus: A central blob (Auditory "Food")
std::vector<float> create_auditory_stimulus() {
    std::vector<float> data(LAYER_SIZE, 0.0f);
    int center = LAYER_DIM / 2;
    data[center * LAYER_DIM + center] = 1.0f;
    data[center * LAYER_DIM + center + 1] = 1.0f;
    data[center * LAYER_DIM + center - 1] = 1.0f;
    data[(center+1) * LAYER_DIM + center] = 1.0f;
    return data;
}

int main() {
    std::cout << "[Genesis] Phase III: Associative Memory & Concepts" << std::endl;

    BioEngine engine;
    engine.init_structured(TOTAL_NEURONS, MAX_SYNAPSES_EST, RNG_SEED);

    // 1. Create Anatomy
    std::cout << "  > Growing Neural Regions..." << std::endl;
    
    // Sensory Periphery
    uint32_t v_in_idx = engine.create_layer("VisInput", LAYER_SIZE, LAYER_ID_INPUT);
    uint32_t a_in_idx = engine.create_layer("AudInput", LAYER_SIZE, LAYER_ID_INPUT);
    
    // Cortex (Standard Plasticity)
    uint32_t v_ctx_idx = engine.create_layer("VisCortex", LAYER_SIZE, LAYER_ID_CORTEX);
    uint32_t a_ctx_idx = engine.create_layer("AudCortex", LAYER_SIZE, LAYER_ID_CORTEX);
    
    // Hippocampus (High Plasticity)
    Hippocampus hippocampus(engine, LAYER_SIZE); // Creates layer internally

    // 2. Wiring (The Connectome)
    std::cout << "  > Wiring Connectome..." << std::endl;
    
    utils::FastRNG rng(RNG_SEED);
    auto& synapses = const_cast<SynapseBlock&>(engine.get_synapses());
    size_t synapse_cursor = 0;

    // A. Sensory -> Cortex (Topographic)
    // "Eyes to Visual Cortex"
    spatial_utils::connect_layers_spatial(
        engine.get_layers().at("VisInput"), engine.get_neurons(),
        engine.get_layers().at("VisCortex"), synapses, synapse_cursor, rng
    );
    // "Ears to Auditory Cortex"
    spatial_utils::connect_layers_spatial(
        engine.get_layers().at("AudInput"), engine.get_neurons(),
        engine.get_layers().at("AudCortex"), synapses, synapse_cursor, rng
    );

    // B. Cortex -> Hippocampus (Feedforward - Encoding)
    // "What I see/hear goes into memory"
    hippocampus.wire_input_from("VisCortex");
    hippocampus.wire_input_from("AudCortex");

    // C. Hippocampus -> Cortex (Feedback - Recall)
    // "Memory triggers imagination/hallucination"
    engine.connect_full("Hippocampus", "VisCortex", W_MAX * 0.3f, 5);
    engine.connect_full("Hippocampus", "AudCortex", W_MAX * 0.3f, 5);

    // D. Hippocampus Recurrence (Attractor)
    // "Completing the pattern"
    hippocampus.enable_associative_memory(0.2f);

    // Finalize
    // Note: hippocampus methods used internal connect calls, so cursor is managed by engine state?
    // Actually, create_layer and connect_full update engine state.
    // spatial_utils manually updates cursor. We need to sync.
    // The previous main.cpp synced it.
    // Let's assume BioEngine's internal cursor is updated by its methods, 
    // and we update it with the manual spatial ones.
    size_t current_engine_cursor = engine.get_synapse_cursor();
    size_t total_cursor = std::max(current_engine_cursor, synapse_cursor);
    
    // Since we mixed manual spatial_utils (using external cursor) and engine methods (internal cursor),
    // we need to be careful. In Phase 2 main.cpp, we passed the cursor.
    // Engine methods (wire_input_from) use the internal cursor.
    // To be safe, we should have updated the engine's cursor after spatial calls *before* calling engine methods.
    // Correct order for this script:
    engine.set_synapse_cursor(synapse_cursor); // Sync spatial work
    
    // Now call engine methods that use the internal cursor
    hippocampus.wire_input_from("VisCortex");
    hippocampus.wire_input_from("AudCortex");
    engine.connect_full("Hippocampus", "VisCortex", W_MAX * 0.3f, 5);
    engine.connect_full("Hippocampus", "AudCortex", W_MAX * 0.3f, 5);
    hippocampus.enable_associative_memory(0.2f);

    engine.bake_topology();
    std::cout << "  > Synapses: " << engine.get_synapse_cursor() << std::endl;

    // 3. Modules
    InputLayer vis_input(engine, engine.get_layers().at("VisInput"));
    InputLayer aud_input(engine, engine.get_layers().at("AudInput"));
    
    CortexLayer vis_cortex(engine, engine.get_layers().at("VisCortex"));
    CortexLayer aud_cortex(engine, engine.get_layers().at("AudCortex"));
    
    Thalamus thalamus_v(engine, engine.get_layers().at("VisInput"));
    Thalamus thalamus_a(engine, engine.get_layers().at("AudInput"));

    auto vis_stimulus = create_visual_stimulus();
    auto aud_stimulus = create_auditory_stimulus();

    std::cout << "[Genesis] Starting Experiment..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < SIM_TICKS; ++t) {
        
        // --- PHASE A: LEARNING (Association) ---
        // Present BOTH stimuli together with Dopamine
        if (t >= 500 && t < 1500) {
            bool pulse = (t % 200) < 100; // 5Hz flicker
            if (pulse) {
                vis_input.present_data(vis_stimulus, 100.0f);
                aud_input.present_data(aud_stimulus, 100.0f);
            }
            engine.set_dopamine(1.0f); // "This is important!"
            engine.set_acetylcholine(2.0f); // "Pay attention!"
        }
        
        // --- GAP: Rest ---
        else if (t >= 1500 && t < 3000) {
            engine.set_dopamine(0.0f);
            engine.set_acetylcholine(1.0f);
        }

        // --- PHASE B: RECALL (Pattern Completion) ---
        // Present VISUAL only. Check if AUDITORY fires.
        else if (t >= 3000 && t < 4000) {
            bool pulse = (t % 200) < 100;
            if (pulse) {
                vis_input.present_data(vis_stimulus, 100.0f);
                // NO AUDITORY INPUT!
            }
            engine.set_dopamine(0.0f); // Testing, not learning
            engine.set_acetylcholine(1.5f); // Focus
        }

        // --- PHASE C: SLEEP (Consolidation) ---
        else if (t >= 4500) {
            engine.set_state(EngineState::SLEEP);
        }

        // Physics Pipeline
        // 1. Thalamic Gating (Only Awake)
        if (engine.get_context().state == EngineState::AWAKE) {
            thalamus_v.apply_gating();
            thalamus_a.apply_gating();
        }

        // 2. Core Physics (Includes Sleep Replay if state == SLEEP)
        engine.tick();

        // 3. Cortical Dynamics (WTA) - Apply even in sleep to sharpen replay
        vis_cortex.apply_lateral_inhibition();
        aud_cortex.apply_lateral_inhibition();
        hippocampus.process_dynamics();

        // Logging
        if (t % LOG_INTERVAL == 0) {
            const auto& ctx = engine.get_context();
            const auto& neurons = engine.get_neurons();
            
            // Count active neurons per region
            int v_in=0, v_ctx=0, a_in=0, a_ctx=0, hip=0;
            
            auto count_layer = [&](const std::string& name) -> int {
                auto meta = engine.get_layers().at(name);
                int c = 0;
                for(uint32_t i=0; i<meta.count; ++i) {
                    if (neurons.has_fired[meta.start_index + i]) c++;
                }
                return c;
            };

            v_in = count_layer("VisInput");
            v_ctx = count_layer("VisCortex");
            a_in = count_layer("AudInput");
            a_ctx = count_layer("AudCortex");
            hip = count_layer("Hippocampus");

            std::string state_str = (ctx.state == EngineState::AWAKE) ? "AWAKE" : "SLEEP";

            std::cout << "T:" << std::setw(4) << t 
                      << " | " << state_str
                      << " | VisIn:" << std::setw(2) << v_in 
                      << " VisCtx:" << std::setw(3) << v_ctx
                      << " | AudIn:" << std::setw(2) << a_in 
                      << " AudCtx:" << std::setw(3) << a_ctx
                      << " | HIP:" << std::setw(3) << hip
                      << " | DA:" << std::fixed << std::setprecision(1) << ctx.dopamine
                      << std::endl;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    
    std::cout << "[Genesis] Experiment Complete." << std::endl;
    std::cout << "  Speedup: " << (SIM_TICKS/1000.0) / elapsed.count() << "x" << std::endl;

    return 0;
}