#pragma once

#include "../types.h"
#include <cmath>
#include <algorithm>

namespace genesis {
namespace spatial_utils {

// -----------------------------------------------------------------------------
// 1. Core Spatial Math
// -----------------------------------------------------------------------------

// Calculates the squared Euclidean distance.
// Faster than Euclidean distance as it avoids the expensive sqrt() operation.
// Perfect for comparisons and radial checks.
inline float euclidean_dist_sq(float x1, float y1, float x2, float y2) {
    const float dx = x1 - x2;
    const float dy = y1 - y2;
    return dx * dx + dy * dy;
}

// -----------------------------------------------------------------------------
// 2. Connectivity Profiles
// -----------------------------------------------------------------------------

// Gaussian (Normal Distribution) Function
// Models the probability or strength of a connection based on distance.
// - dist_sq: Squared distance between neurons.
// - sigma_sq: Squared standard deviation (controls the "spread" or radius of connections).
// Returns a value in [0, 1].
inline float gaussian_profile(float dist_sq, float sigma_sq) {
    if (sigma_sq < 1e-9) return 0.0f; // Avoid division by zero
    return std::exp(-dist_sq / (2.0f * sigma_sq));
}


// "Difference of Gaussians" or Mexican Hat Function
// This is the mathematical model for center-surround receptive fields,
// which is the basis of Lateral Inhibition.
constexpr float INPUT_SYNAPTIC_STRENGTH_SCALE = 15.0f;
// - A strong, narrow excitatory center (positive Gaussian).
// - A weaker, wider inhibitory surround (negative Gaussian).
// Returns a value typically in [-1, 1].
inline float mexican_hat_profile(float dist_sq, 
                                 float sigma_excite_sq, 
                                 float sigma_inhibit_sq,
                                 float strength_excite,
                                 float strength_inhibit) 
{
    float excite = strength_excite * gaussian_profile(dist_sq, sigma_excite_sq);
    float inhibit = strength_inhibit * gaussian_profile(dist_sq, sigma_inhibit_sq);
    return excite - inhibit;
}


// -----------------------------------------------------------------------------
// 3. Topology Builder Helpers
// -----------------------------------------------------------------------------

// Connects two layers using a specific connectivity profile.
// This function encapsulates the logic of iterating through neuron pairs,
// calculating their distance, and creating a synapse based on the result.
inline void connect_layers_spatial(
    const LayerMeta& pre_meta, const NeuronBlock& neurons,
    const LayerMeta& post_meta,
    SynapseBlock& synapses, size_t& synapse_cursor,
    utils::FastRNG& rng) 
{
    // Example implementation for a simple Gaussian falloff
    const float sigma = 2.0f; // Connection radius in grid units
    const float sigma_sq = sigma * sigma;
    const float connection_prob_threshold = 0.1f;

    for (uint32_t i = 0; i < post_meta.count; ++i) {
        uint32_t post_id = post_meta.start_index + i;
        float post_x = neurons.pos_x[post_id];
        float post_y = neurons.pos_y[post_id];

        for (uint32_t j = 0; j < pre_meta.count; ++j) {
            uint32_t pre_id = pre_meta.start_index + j;
            float pre_x = neurons.pos_x[pre_id];
            float pre_y = neurons.pos_y[pre_id];
            
            float dist_sq = euclidean_dist_sq(pre_x, pre_y, post_x, post_y);
            float prob = gaussian_profile(dist_sq, sigma_sq);

            if (prob > connection_prob_threshold && prob > rng.next_float()) {
                if (synapse_cursor >= synapses.weights.capacity()) return;

                synapses.pre_indices[synapse_cursor] = pre_id;
                synapses.post_indices[synapse_cursor] = post_id;
                // Weight is proportional to the connection probability
                synapses.weights[synapse_cursor] = prob * INPUT_SYNAPTIC_STRENGTH_SCALE;
                synapses.delays[synapse_cursor] = 1 + rng.next_int(0, 4); // Small random delay
                synapses.is_inhibitory[synapse_cursor] = false;
                synapses.eligibility_traces[synapse_cursor] = 0.0f;
                synapse_cursor++;
            }
        }
    }
}


} // namespace spatial_utils
} // namespace genesis