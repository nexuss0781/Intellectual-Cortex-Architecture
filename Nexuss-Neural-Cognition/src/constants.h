#pragma once

#include <cstddef> // for size_t
#include <cstdint>

namespace genesis {

// -----------------------------------------------------------------------------
// 1. Simulation Time Base (Phase I)
// -----------------------------------------------------------------------------
constexpr float DT_MS = 1.0f;


// -----------------------------------------------------------------------------
// 2. Membrane Dynamics (Phase I)
// -----------------------------------------------------------------------------
constexpr float V_REST_MV       = -70.0f;
constexpr float V_RESET_MV      = -70.0f;
constexpr float V_THRESH_MV     = -55.0f;

constexpr float MEMBRANE_TAU_MS = 20.0f;
constexpr float VOLTAGE_DECAY_FACTOR = 0.951229f; // exp(-1.0 / 20.0)

constexpr int   REFRACTORY_TICKS     = 5;


// -----------------------------------------------------------------------------
// 3. Metabolic Constraints (Phase I)
// -----------------------------------------------------------------------------
constexpr float ATP_MAX         = 1.0f;
constexpr float ATP_REFILL_RATE = 0.01f; 
constexpr float SPIKE_COST      = 0.10f; 
constexpr float ATP_MIN_TO_FIRE = 0.10f;


// -----------------------------------------------------------------------------
// 4. Synaptic Plasticity (Phase I)
// -----------------------------------------------------------------------------
constexpr float STDP_WINDOW_MS  = 20.0f;
constexpr float A_PLUS          = 0.10f; 
constexpr float A_MINUS         = 0.12f; 

constexpr float TRACE_TAU_MS    = 100.0f;
constexpr float TRACE_DECAY_FACTOR = 0.990050f;


// -----------------------------------------------------------------------------
// 5. Neuromodulation (Phase I)
// -----------------------------------------------------------------------------
constexpr float DOPAMINE_TAU_MS = 200.0f;
constexpr float DOPAMINE_DECAY_FACTOR = 0.995012f;


// -----------------------------------------------------------------------------
// 6. Structural Bounds (Phase I)
// -----------------------------------------------------------------------------
constexpr float W_MIN           = 0.0001f;
constexpr float W_MAX           = 1.0f;

constexpr size_t MAX_NEURONS    = 20000; 
constexpr size_t MAX_SYNAPSES   = MAX_NEURONS * 100;


// -----------------------------------------------------------------------------
// 7. Phase II: Sensory & Spatial Constants
// -----------------------------------------------------------------------------
constexpr size_t MAX_DELAY_TICKS = 20;

constexpr size_t INPUT_WIDTH  = 28;
constexpr size_t INPUT_HEIGHT = 28;
constexpr size_t INPUT_SIZE   = INPUT_WIDTH * INPUT_HEIGHT;

constexpr float INHIBITION_RADIUS   = 3.0f; 
constexpr float INHIBITION_STRENGTH = 5.0f; 


// -----------------------------------------------------------------------------
// 8. Phase III: Memory & States (NEW)
// -----------------------------------------------------------------------------

// Layer Identities
constexpr uint8_t LAYER_ID_INPUT       = 0;
constexpr uint8_t LAYER_ID_THALAMUS    = 1; // Virtual/Placeholder
constexpr uint8_t LAYER_ID_CORTEX      = 2; // Slow Learning
constexpr uint8_t LAYER_ID_HIPPOCAMPUS = 3; // Fast Learning (One-Shot)
constexpr uint8_t LAYER_ID_ASSOCIATION = 4; // Multimodal Integration

// Plasticity Scalars (Learning Rate Multipliers)
// Hippocampus learns 10x faster than Cortex to enable episodic capture.
constexpr float PLASTICITY_SCALE_CORTEX      = 1.0f;
constexpr float PLASTICITY_SCALE_HIPPOCAMPUS = 10.0f;

// Sleep / Replay Dynamics
// During sleep, we inject random noise to trigger "replay" chains in the Hippocampus.
constexpr float SLEEP_REPLAY_NOISE_PROB = 0.001f; // Probability per tick per neuron
constexpr float SLEEP_REPLAY_NOISE_MAG  = 15.0f;  // Magnitude of spontaneous firing (enough to trigger)
constexpr float SLEEP_SENSORY_GATING    = 0.0f;   // Input gain during sleep (0 = Blind)

} // namespace genesis