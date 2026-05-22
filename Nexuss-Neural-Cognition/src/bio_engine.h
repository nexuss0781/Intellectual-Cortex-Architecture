#pragma once

#include "constants.h"
#include "types.h"
#include "utils.h"

#include <vector>
#include <memory>
#include <string>
#include <map>

namespace genesis {

// Helper struct to track functional regions
struct LayerMeta {
    uint32_t start_index;
    uint32_t count;
    uint8_t  layer_id;
    // Phase III: Metadata for plasticity (Fast vs Slow learning regions)
    float    plasticity_scale; 
};

class BioEngine {
public:
    BioEngine() = default;
    ~BioEngine() = default;

    // -------------------------------------------------------------------------
    // API: Lifecycle & Construction
    // -------------------------------------------------------------------------

    void init(size_t neuron_count, size_t synapse_density, uint64_t seed);
    void init_structured(size_t max_neurons, size_t max_synapses, uint64_t seed);

    // Phase III Update: Added plasticity_scale
    // Default 1.0f = Standard Cortex. 
    // Use 10.0f for Hippocampus (One-Shot Learning).
    uint32_t create_layer(const std::string& name, size_t count, uint8_t layer_type, float plasticity_scale = 1.0f);

    size_t connect_full(const std::string& pre_name, const std::string& post_name, 
                        float weight_scale, uint8_t delay_ticks);

    // Phase III New: Connect a layer to itself.
    // Creates Recurrent Collaterals essential for Pattern Completion (Attractor Dynamics).
    // density: 0.0 to 1.0 (Fraction of possible self-connections to create).
    size_t connect_recurrent(const std::string& layer_name, float weight_scale, 
                             uint8_t delay_ticks, float density);

    void bake_topology();

    // -------------------------------------------------------------------------
    // API: Simulation & State Control
    // -------------------------------------------------------------------------

    void tick();

    // Phase III New: Switch between AWAKE (Learning) and SLEEP (Consolidation).
    void set_state(EngineState state);

    // -------------------------------------------------------------------------
    // API: Interaction
    // -------------------------------------------------------------------------
    void inject_stimulus(uint32_t neuron_id, float current_mv);
    void set_dopamine(float level);
    
    // Phase III: Acetylcholine controls input gain (Attention)
    void set_acetylcholine(float level);

    // --- Accessors ---
    const NeuronBlock& get_neurons() const { return neurons_; }
    NeuronBlock& get_neurons() { return neurons_; }
    const SynapseBlock& get_synapses() const { return synapses_; }
    const Context& get_context() const { return ctx_; }
    size_t get_synapse_cursor() const { return synapse_cursor_; }
    void set_synapse_cursor(size_t cursor) { synapse_cursor_ = cursor; }
    const std::map<std::string, LayerMeta>& get_layers() const { return layers_; }

private:
    // -------------------------------------------------------------------------
    // Physics Modules
    // -------------------------------------------------------------------------
    void update_metabolism();
    
    // Phase III Update: Now uses per-neuron plasticity_scale
    void update_synapses();
    
    std::vector<uint32_t> integrate_and_fire();
    void propagate_spikes(const std::vector<uint32_t>& firing_indices);
    void process_delayed_spikes();
    
    // Process immediate spikes (delay=0) after propagation
    void process_immediate_spikes();

    // Phase III New: Spontaneous activity generator.
    // In SLEEP mode, injects noise to trigger "Replay" (Sharp-Wave Ripples).
    void process_sleep_dynamics();

    // -------------------------------------------------------------------------
    // Data
    // -------------------------------------------------------------------------
    NeuronBlock   neurons_;
    SynapseBlock  synapses_;
    TopologyIndex topology_;
    Context       ctx_;
    
    SpikeDelayQueue delay_queue_;
    std::map<std::string, LayerMeta> layers_;
    
    size_t neuron_cursor_ = 0; 
    size_t synapse_cursor_ = 0;
    
    std::unique_ptr<utils::FastRNG> rng_;
    size_t neuron_count_ = 0;
};

} // namespace genesis