#pragma once

#include "../bio_engine.h"
#include "spatial_utils.h"

namespace genesis {

// -----------------------------------------------------------------------------
// CortexLayer: The "Visual Cortex" (V1)
// -----------------------------------------------------------------------------
// Purpose: A self-organizing sheet of neurons that learns to extract features
// from its input. It is defined by its internal competitive dynamics.
class CortexLayer {
public:
    CortexLayer(BioEngine& engine, const LayerMeta& layer_meta)
        : engine_(engine), meta_(layer_meta) {}

    // Main API: Apply lateral inhibition to all neurons within this layer.
    // This function should be called by the main simulation loop on every tick
    // AFTER the standard integration step.
    void apply_lateral_inhibition() {
        const auto& neurons = engine_.get_neurons();
        auto& mutable_neurons = const_cast<NeuronBlock&>(engine_.get_neurons());
        
        // Find who fired in our layer this tick
        std::vector<uint32_t> firing_in_layer;
        for (uint32_t i = 0; i < meta_.count; ++i) {
            uint32_t neuron_id = meta_.start_index + i;
            if (neurons.has_fired[neuron_id]) {
                firing_in_layer.push_back(neuron_id);
            }
        }
        
        if (firing_in_layer.empty()) return;

        // For each neuron that fired, inhibit its spatial neighbors.
        // We need to find the "winner" (strongest stimulus) first
        uint32_t winner_id = firing_in_layer[0];
        float max_potential = neurons.membrane_potential[winner_id];
        for (uint32_t fid : firing_in_layer) {
            if (neurons.membrane_potential[fid] > max_potential) {
                max_potential = neurons.membrane_potential[fid];
                winner_id = fid;
            }
        }
        
        // Only the winner inhibits others
        float firing_x = neurons.pos_x[winner_id];
        float firing_y = neurons.pos_y[winner_id];

        for (uint32_t i = 0; i < meta_.count; ++i) {
            uint32_t target_id = meta_.start_index + i;
            if (winner_id == target_id) continue; // Don't inhibit self

            float target_x = neurons.pos_x[target_id];
            float target_y = neurons.pos_y[target_id];

            // Calculate inhibition strength based on distance (Gaussian falloff)
            float dist_sq = spatial_utils::euclidean_dist_sq(firing_x, firing_y, target_x, target_y);
            float radius_sq = INHIBITION_RADIUS * INHIBITION_RADIUS;

            if (dist_sq < radius_sq) {
                float strength = INHIBITION_STRENGTH * spatial_utils::gaussian_profile(dist_sq, radius_sq / 2.0f);
                
                // Apply inhibitory current
                // This directly reduces the membrane potential of neighbors, making them less likely to fire.
                mutable_neurons.membrane_potential[target_id] -= strength;
            }
        }
        
        // Reset winner to V_RESET after inhibition is applied
        // Winner has already fired and should be at reset potential
        mutable_neurons.membrane_potential[winner_id] = V_RESET_MV;
    }

private:
    BioEngine& engine_;
    LayerMeta meta_;
};

} // namespace genesis