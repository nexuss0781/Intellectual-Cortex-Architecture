#pragma once

#include "../bio_engine.h"
#include "../sensory/spatial_utils.h"

namespace genesis {

// -----------------------------------------------------------------------------
// Hippocampus (The Fast Learner)
// -----------------------------------------------------------------------------
// Biological Role: Rapidly encodes episodic memories ("snapshots" of cortical activity).
// Key Features:
// 1. High Plasticity: Synapses change 10x faster than Cortex.
// 2. Recurrence (CA3): Neurons connect to each other to form "Attractors" (Pattern Completion).
// 3. Sparsity (DG): Strong inhibition ensures distinct events have distinct neural codes.
class Hippocampus {
public:
    Hippocampus(BioEngine& engine, size_t size)
        : engine_(engine) 
    {
        // 1. Create the layer with High Plasticity
        // This is the physical key to One-Shot Learning.
        layer_start_ = engine_.create_layer(
            "Hippocampus", 
            size, 
            LAYER_ID_HIPPOCAMPUS, 
            PLASTICITY_SCALE_HIPPOCAMPUS // 10.0f
        );
        layer_count_ = size;
    }

    // Connects the slow Cortex (Source) to the fast Hippocampus (Target).
    // This allows the Hippocampus to take a "snapshot" of the Cortex's state.
    void wire_input_from(const std::string& cortex_layer_name) {
        // Random projection is sufficient for indexing (Dentate Gyrus style)
        // Sparse connectivity (e.g., 10-20% chance)
        size_t connections = engine_.connect_full(
            cortex_layer_name, "Hippocampus", 
            W_MAX * 0.5f, // Initial weights
            2             // 2 tick delay (Axonal transmission time)
        );
        (void)connections; // Squelch unused var warning
    }

    // Enables "Attractor Dynamics" (CA3).
    // Connects Hippocampal neurons to each other.
    // If the network sees "Half of Pattern A", these links pull it to "Full Pattern A".
    void enable_associative_memory(float density = 0.2f) {
        engine_.connect_recurrent(
            "Hippocampus", 
            W_MAX * 0.2f, // Weak initial recurrent weights (they grow fast!)
            1,            // Fast local feedback
            density
        );
    }

    // Main Loop Logic: Enforce Sparsity
    // Called every tick. Applies strong Lateral Inhibition.
    // This ensures only the "strongest" neurons represent the memory, 
    // preventing the memory from becoming "muddy" (Pattern Separation).
    void process_dynamics() {
        auto& neurons = const_cast<NeuronBlock&>(engine_.get_neurons());
        
        // Find firing neurons
        std::vector<uint32_t> active_indices;
        for (uint32_t i = 0; i < layer_count_; ++i) {
            uint32_t idx = layer_start_ + i;
            if (neurons.has_fired[idx]) {
                active_indices.push_back(idx);
            }
        }

        if (active_indices.empty()) return;

        // Apply Global Inhibition (simplified Dentate Gyrus logic)
        // If too many neurons fire, suppress the weaker ones in the neighborhood.
        // We use the spatial utils from Phase II.
        for (uint32_t firing_id : active_indices) {
            float x1 = neurons.pos_x[firing_id];
            float y1 = neurons.pos_y[firing_id];

            for (uint32_t i = 0; i < layer_count_; ++i) {
                uint32_t target_id = layer_start_ + i;
                if (firing_id == target_id) continue;

                float x2 = neurons.pos_x[target_id];
                float y2 = neurons.pos_y[target_id];

                float dist_sq = spatial_utils::euclidean_dist_sq(x1, y1, x2, y2);
                
                // Hippocampus has a WIDER inhibition radius than Cortex to ensure sparsity
                float radius_sq = (INHIBITION_RADIUS * 2.0f) * (INHIBITION_RADIUS * 2.0f);

                if (dist_sq < radius_sq) {
                    // Strong inhibition
                    neurons.membrane_potential[target_id] -= INHIBITION_STRENGTH * 1.5f;
                }
            }
        }
    }

private:
    BioEngine& engine_;
    uint32_t layer_start_;
    size_t layer_count_;
};

} // namespace genesis