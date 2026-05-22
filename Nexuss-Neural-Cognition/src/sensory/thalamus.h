#pragma once

#include "../bio_engine.h"

namespace genesis {

// -----------------------------------------------------------------------------
// Thalamus: The Attentional Gate
// -----------------------------------------------------------------------------
// V2: Corrected logic to be predictive, not reactive.
// It now clamps voltage *before* the spike can happen.
class Thalamus {
public:
    Thalamus(BioEngine& engine, const LayerMeta& layer_meta)
        : engine_(engine), 
          meta_(layer_meta),
          activity_avg_(meta_.count, 0.0f) 
    {}

    // Main API: Apply attentional gating logic.
    // This function must be called BEFORE engine.tick().
    void apply_gating() {
        auto& neurons = const_cast<NeuronBlock&>(engine_.get_neurons());
        const auto& ctx = engine_.get_context();

        // 1. Determine "surprise" threshold based on global attention (Acetylcholine).
        const float surprise_threshold = 20.0f / (ctx.acetylcholine + 0.01f);

        for (uint32_t i = 0; i < meta_.count; ++i) {
            uint32_t neuron_id = meta_.start_index + i;
            float& current_potential = neurons.membrane_potential[neuron_id];

            // 2. Update the moving average of this neuron's activity.
            // This represents the neuron's expectation of its own input.
            activity_avg_[i] = (activity_avg_[i] * 0.99f) + (current_potential * 0.01f);
            
            // 3. Predictive Gating Logic
            // If the neuron is about to fire (potential is above threshold)...
            if (current_potential >= V_THRESH_MV) {
                
                // ...is the spike "surprising" enough to pass the gate?
                bool is_surprising = (current_potential > (activity_avg_[i] + surprise_threshold));
                
                if (!is_surprising) {
                    // VETO! The spike is predictable/boring.
                    // Prevent it from firing by clamping its voltage back to rest.
                    // The main engine's firing check will now fail for this neuron.
                    current_potential = V_RESET_MV;
                }
                // If it IS surprising, we do nothing and let the engine fire it normally.
            }
        }
    }

private:
    BioEngine& engine_;
    LayerMeta meta_;
    std::vector<float> activity_avg_;
};

} // namespace genesis