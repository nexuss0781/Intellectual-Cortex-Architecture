#pragma once

#include "../bio_engine.h"
#include <vector>

namespace genesis {

// -----------------------------------------------------------------------------
// InputLayer: The "Retina"
// -----------------------------------------------------------------------------
// Purpose: Converts static, digital data (like a 2D image) into dynamic, 
// biological spike trains. It does not learn or have complex dynamics. 
// It is a pure transducer.
class InputLayer {
public:
    // Takes a reference to the main engine to directly inject stimulus.
    // layer_meta provides the indices of the neurons this layer controls.
    InputLayer(BioEngine& engine, const LayerMeta& layer_meta)
        : engine_(engine), meta_(layer_meta), rng_(engine.get_context().rng_seed) {}

    // Main API: Present a 2D array of intensity values to the input neurons.
    // - data: Flat vector of intensity values [0.0, 1.0], row-major order.
    // - max_fire_rate_hz: The firing rate (spikes/sec) for a pixel with intensity 1.0.
    void present_data(const std::vector<float>& data, float max_fire_rate_hz) {
        if (data.size() != meta_.count) {
            // Error: Input data size does not match layer neuron count.
            return;
        }

        // Convert firing rate in Hz to a probability per millisecond tick.
        // e.g., 100 Hz = 100 spikes / 1000 ms = 0.1 probability per tick.
        const float prob_per_tick_scale = max_fire_rate_hz / 1000.0f;

        for (uint32_t i = 0; i < meta_.count; ++i) {
            float intensity = data[i];
            if (intensity <= 0.0f) continue;

            // Poisson Spike Train Generation:
            // A random number is compared against the target probability.
            // This creates a variable, "natural" looking spike train.
            if (rng_.next_float() < (intensity * prob_per_tick_scale)) {
                // If the check passes, inject a strong, super-threshold stimulus
                // to guarantee the corresponding neuron fires on this tick.
                uint32_t neuron_id = meta_.start_index + i;
                engine_.inject_stimulus(neuron_id, 50.0f); // Strong pulse
            }
        }
    }

private:
    BioEngine& engine_;       // Reference to the main simulation engine.
    LayerMeta meta_;          // The start index and count of our neurons.
    utils::FastRNG rng_;      // A local RNG for generating spike trains.
};

} // namespace genesis