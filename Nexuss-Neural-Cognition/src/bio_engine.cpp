#include "bio_engine.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <numeric>

namespace genesis {

// -----------------------------------------------------------------------------
// Initialization (Phase I - Legacy)
// -----------------------------------------------------------------------------
void BioEngine::init(size_t neuron_count, size_t synapse_density_per_neuron, uint64_t seed) {
    init_structured(neuron_count, neuron_count * synapse_density_per_neuron, seed);
    create_layer("Global", neuron_count, 2); 
    
    // Legacy Random Topology
    size_t est_synapse_count = neuron_count * synapse_density_per_neuron;
    if (est_synapse_count > MAX_SYNAPSES) est_synapse_count = MAX_SYNAPSES;

    synapse_cursor_ = 0; 
    
    for (size_t pre_id = 0; pre_id < neuron_count; ++pre_id) {
        size_t n_connections = synapse_density_per_neuron;
        if (synapse_cursor_ + n_connections > est_synapse_count) {
            n_connections = est_synapse_count - synapse_cursor_;
        }

        for (size_t c = 0; c < n_connections; ++c) {
            uint32_t post_id = rng_->next_int(0, static_cast<int>(neuron_count - 1));
            if (post_id == pre_id) post_id = (post_id + 1) % neuron_count;

            synapses_.pre_indices[synapse_cursor_]  = static_cast<uint32_t>(pre_id);
            synapses_.post_indices[synapse_cursor_] = post_id;
            synapses_.weights[synapse_cursor_]            = rng_->next_float() * 0.1f + 0.01f; 
            synapses_.eligibility_traces[synapse_cursor_] = 0.0f;
            synapses_.is_inhibitory[synapse_cursor_]      = (rng_->next_float() < 0.2f);
            synapses_.delays[synapse_cursor_]             = 1; 

            synapse_cursor_++;
        }
        if (synapse_cursor_ >= est_synapse_count) break;
    }
    bake_topology();
}

// -----------------------------------------------------------------------------
// Initialization (Phase II/III - Structured)
// -----------------------------------------------------------------------------
void BioEngine::init_structured(size_t max_neurons, size_t max_synapses, uint64_t seed) {
    neuron_count_ = max_neurons; 
    rng_ = std::make_unique<utils::FastRNG>(seed);
    ctx_.rng_seed = seed;
    ctx_.current_tick = 100; 
    ctx_.state = EngineState::AWAKE;

    neurons_.resize(max_neurons);
    synapses_.resize(max_synapses);
    topology_.resize(max_neurons);

    for (size_t i = 0; i < max_neurons; ++i) {
        neurons_.membrane_potential[i] = V_REST_MV;
        neurons_.recovery_variable[i] = 0.0f;
        neurons_.atp_level[i] = ATP_MAX;
        neurons_.refractory_timer[i] = 0;
        neurons_.has_fired[i] = false;
        neurons_.last_spike_time[i] = 0; 
        neurons_.avg_firing_rate[i] = 0.0f;
        neurons_.pos_x[i] = 0.0f;
        neurons_.pos_y[i] = 0.0f;
        neurons_.layer_id[i] = 0;
        // Phase III Default
        neurons_.plasticity_scale[i] = PLASTICITY_SCALE_CORTEX; 
    }

    neuron_cursor_ = 0;
    synapse_cursor_ = 0;
    layers_.clear();
    delay_queue_.clear();
}

uint32_t BioEngine::create_layer(const std::string& name, size_t count, uint8_t layer_type, float plasticity_scale) {
    if (neuron_cursor_ + count > neurons_.membrane_potential.size()) {
        std::cerr << "[BioEngine] Error: Neuron capacity exceeded creating layer " << name << std::endl;
        return 0;
    }

    uint32_t start_idx = static_cast<uint32_t>(neuron_cursor_);
    
    // Register Layer with plasticity metadata
    layers_[name] = { start_idx, static_cast<uint32_t>(count), layer_type, plasticity_scale };

    // Initialize Metadata
    for (size_t i = 0; i < count; ++i) {
        size_t idx = start_idx + i;
        neurons_.layer_id[idx] = layer_type;
        neurons_.plasticity_scale[idx] = plasticity_scale;
    }

    neuron_cursor_ += count;
    return start_idx;
}

size_t BioEngine::connect_full(const std::string& pre_name, const std::string& post_name, 
                               float weight_scale, uint8_t delay_ticks) {
    if (layers_.find(pre_name) == layers_.end() || layers_.find(post_name) == layers_.end()) {
        return 0;
    }

    LayerMeta pre = layers_[pre_name];
    LayerMeta post = layers_[post_name];

    size_t created = 0;

    for (uint32_t i = 0; i < pre.count; ++i) {
        for (uint32_t j = 0; j < post.count; ++j) {
            if (synapse_cursor_ >= synapses_.weights.size()) break;

            synapses_.pre_indices[synapse_cursor_]  = pre.start_index + i;
            synapses_.post_indices[synapse_cursor_] = post.start_index + j;
            synapses_.weights[synapse_cursor_]      = weight_scale * (0.9f + rng_->next_float() * 0.2f);
            synapses_.eligibility_traces[synapse_cursor_] = 0.0f;
            synapses_.is_inhibitory[synapse_cursor_] = false; 
            synapses_.delays[synapse_cursor_] = (delay_ticks > 0) ? delay_ticks : 1;

            synapse_cursor_++;
            created++;
        }
    }
    return created;
}

size_t BioEngine::connect_recurrent(const std::string& layer_name, float weight_scale, 
                                    uint8_t delay_ticks, float density) {
    if (layers_.find(layer_name) == layers_.end()) return 0;

    LayerMeta layer = layers_[layer_name];
    size_t created = 0;

    for (uint32_t i = 0; i < layer.count; ++i) {
        for (uint32_t j = 0; j < layer.count; ++j) {
            // Don't connect to self
            if (i == j) continue;

            // Random sparsity
            if (rng_->next_float() > density) continue;

            if (synapse_cursor_ >= synapses_.weights.size()) break;

            synapses_.pre_indices[synapse_cursor_]  = layer.start_index + i;
            synapses_.post_indices[synapse_cursor_] = layer.start_index + j;
            synapses_.weights[synapse_cursor_]      = weight_scale * (0.9f + rng_->next_float() * 0.2f);
            synapses_.eligibility_traces[synapse_cursor_] = 0.0f;
            synapses_.is_inhibitory[synapse_cursor_] = false; 
            synapses_.delays[synapse_cursor_] = (delay_ticks > 0) ? delay_ticks : 1;

            synapse_cursor_++;
            created++;
        }
    }
    return created;
}

void BioEngine::bake_topology() {
    std::fill(topology_.outgoing_count.begin(), topology_.outgoing_count.end(), 0);
    std::fill(topology_.outgoing_start.begin(), topology_.outgoing_start.end(), 0);

    size_t count = synapse_cursor_;
    std::vector<size_t> p(count);
    std::iota(p.begin(), p.end(), 0);

    std::sort(p.begin(), p.end(), [&](size_t a, size_t b) {
        return synapses_.pre_indices[a] < synapses_.pre_indices[b];
    });

    SynapseBlock sorted_syn;
    sorted_syn.resize(count);

    for (size_t i = 0; i < count; ++i) {
        size_t src = p[i];
        sorted_syn.pre_indices[i]        = synapses_.pre_indices[src];
        sorted_syn.post_indices[i]       = synapses_.post_indices[src];
        sorted_syn.weights[i]            = synapses_.weights[src];
        sorted_syn.eligibility_traces[i] = synapses_.eligibility_traces[src];
        sorted_syn.is_inhibitory[i]      = synapses_.is_inhibitory[src];
        sorted_syn.delays[i]             = synapses_.delays[src];
    }

    for (size_t i = 0; i < count; ++i) {
        synapses_.pre_indices[i]        = sorted_syn.pre_indices[i];
        synapses_.post_indices[i]       = sorted_syn.post_indices[i];
        synapses_.weights[i]            = sorted_syn.weights[i];
        synapses_.eligibility_traces[i] = sorted_syn.eligibility_traces[i];
        synapses_.is_inhibitory[i]      = sorted_syn.is_inhibitory[i];
        synapses_.delays[i]             = sorted_syn.delays[i];
    }

    for (size_t k = 0; k < count; ++k) {
        uint32_t pre = synapses_.pre_indices[k];
        if (topology_.outgoing_count[pre] == 0) {
            topology_.outgoing_start[pre] = static_cast<uint32_t>(k);
        }
        topology_.outgoing_count[pre]++;
    }
}

// -----------------------------------------------------------------------------
// Core Physics Loop
// -----------------------------------------------------------------------------
void BioEngine::tick() {
    ctx_.current_tick++;
    ctx_.total_spikes_this_tick = 0;
    
    // Decay global dopamine
    ctx_.dopamine *= DOPAMINE_DECAY_FACTOR;
    if (ctx_.dopamine < 0.001f) ctx_.dopamine = 0.0f;

    // 1. Process Delayed Spikes (Axonal Transmission) - decrement timers, deliver those that reach 0
    process_delayed_spikes();

    // 2. Sleep Dynamics (Phase III)
    if (ctx_.state == EngineState::SLEEP) {
        process_sleep_dynamics();
    }

    // 3. Standard Physics
    update_metabolism();
    update_synapses();
    
    auto firing_indices = integrate_and_fire();
    ctx_.total_spikes_this_tick = firing_indices.size();

    propagate_spikes(firing_indices);
    
    // 4. Process immediate spikes (delay=0) for next tick's integration
    // These are added to membrane potential and will be considered in next tick's integrate_and_fire
    process_immediate_spikes();
}

// -----------------------------------------------------------------------------
// Physics Modules
// -----------------------------------------------------------------------------

void BioEngine::process_delayed_spikes() {
    auto it = std::remove_if(delay_queue_.begin(), delay_queue_.end(), 
        [this](PendingSpike& s) {
            if (s.ticks_remaining > 0) s.ticks_remaining--;
            if (s.ticks_remaining == 0) {
                neurons_.membrane_potential[s.target_neuron_id] += s.signal_strength;
                return true; 
            }
            return false; 
        });
    delay_queue_.erase(it, delay_queue_.end());
}

void BioEngine::process_immediate_spikes() {
    // Process spikes with 0 delay immediately (before integration)
    auto it = std::remove_if(delay_queue_.begin(), delay_queue_.end(), 
        [this](PendingSpike& s) {
            if (s.ticks_remaining == 0) {
                neurons_.membrane_potential[s.target_neuron_id] += s.signal_strength;
                return true; 
            }
            return false; 
        });
    delay_queue_.erase(it, delay_queue_.end());
}

void BioEngine::process_sleep_dynamics() {
    // Sharp-Wave Ripples: Spontaneous firing during sleep
    // Iterate over neurons (specifically Hippocampus ideally, but global for Phase III start)
    const size_t cnt = neuron_cursor_ > 0 ? neuron_cursor_ : neuron_count_;
    
    for (size_t i = 0; i < cnt; ++i) {
        // If this neuron belongs to the Hippocampus (Layer 3), it's more unstable during sleep
        float prob = SLEEP_REPLAY_NOISE_PROB;
        
        // Hippocampus is more likely to self-activate
        if (neurons_.layer_id[i] == LAYER_ID_HIPPOCAMPUS) {
            prob *= 5.0f; 
        }

        if (rng_->next_float() < prob) {
            neurons_.membrane_potential[i] += SLEEP_REPLAY_NOISE_MAG;
        }
    }
}

void BioEngine::update_metabolism() {
    const size_t cnt = neuron_cursor_ > 0 ? neuron_cursor_ : neuron_count_;
    for (size_t i = 0; i < cnt; ++i) {
        float& atp = neurons_.atp_level[i];
        atp += ATP_REFILL_RATE;
        if (atp > 1.0f) atp = 1.0f;
    }
}

void BioEngine::update_synapses() {
    const size_t syn_count = synapse_cursor_ > 0 ? synapse_cursor_ : synapses_.weights.size();
    const float da = ctx_.dopamine;
    const bool has_dopamine = (da > 0.0001f);

    for (size_t k = 0; k < syn_count; ++k) {
        float& trace = synapses_.eligibility_traces[k];
        trace *= TRACE_DECAY_FACTOR;

        if (has_dopamine) {
            uint32_t post_id = synapses_.post_indices[k];
            
            // Phase III: Modulate learning rate by the Post-Neuron's plasticity scale
            // Hippocampus (10x) vs Cortex (1x)
            float plasticity = neurons_.plasticity_scale[post_id];
            
            float& w = synapses_.weights[k];
            float delta_w = da * trace * plasticity; 
            w += delta_w;

            if (w > W_MAX) w = W_MAX;
            else if (w < W_MIN) w = W_MIN;
        }
    }
}

std::vector<uint32_t> BioEngine::integrate_and_fire() {
    std::vector<uint32_t> fired;
    fired.reserve(neuron_count_ / 10); 

    const size_t cnt = neuron_cursor_ > 0 ? neuron_cursor_ : neuron_count_;
    
    for (size_t i = 0; i < cnt; ++i) {
        neurons_.has_fired[i] = false;

        if (neurons_.refractory_timer[i] > 0) {
            neurons_.refractory_timer[i]--;
            continue; 
        }

        // Layer-specific logic
        if (neurons_.layer_id[i] == LAYER_ID_INPUT) {
            // Input layer is forced by inject_stimulus.
            // If we are in SLEEP mode, sensory input should be gated.
            if (ctx_.state == EngineState::SLEEP) {
                // Decay voltage to rest to ignore external stimuli
                neurons_.membrane_potential[i] = V_REST_MV;
            } else {
                if (neurons_.membrane_potential[i] > V_THRESH_MV) {
                    neurons_.membrane_potential[i] = V_THRESH_MV + 1.0f; 
                }
            }
        }

        float& v = neurons_.membrane_potential[i];
        bool did_fire = false;

        if (v >= V_THRESH_MV) {
            if (neurons_.atp_level[i] >= ATP_MIN_TO_FIRE) {
                fired.push_back(static_cast<uint32_t>(i));
                neurons_.has_fired[i] = true;
                did_fire = true;
            }
        }

        // If the neuron did not fire, its potential leaks back to rest.
        // If it did fire, it gets reset later in propagate_spikes.
        if (!did_fire) {
            float v_diff = v - V_REST_MV;
            v_diff *= VOLTAGE_DECAY_FACTOR;
            v = V_REST_MV + v_diff;
        }
    }
    return fired;
}

void BioEngine::propagate_spikes(const std::vector<uint32_t>& firing_indices) {
    // Phase III: ACh varies. During sleep, it might be low or high depending on stage.
    // For simplicity, we assume set_state or set_acetylcholine manages this.
    const float ach_gain = ctx_.acetylcholine;
    
    for (uint32_t pre_id : firing_indices) {
        neurons_.membrane_potential[pre_id] = V_RESET_MV;
        neurons_.refractory_timer[pre_id]   = REFRACTORY_TICKS;
        neurons_.atp_level[pre_id]          -= SPIKE_COST;
        neurons_.last_spike_time[pre_id]    = ctx_.current_tick;

        uint32_t start = topology_.outgoing_start[pre_id];
        uint32_t count = topology_.outgoing_count[pre_id];

        for (uint32_t k = start; k < start + count; ++k) {
            uint32_t post_id = synapses_.post_indices[k];
            
            float w = synapses_.weights[k];
            bool inhib = synapses_.is_inhibitory[k];
            uint8_t delay = synapses_.delays[k];
            
            float signal = w * ach_gain;
            if (inhib) signal = -signal;

            delay_queue_.push_back({post_id, signal, delay});

            synapses_.eligibility_traces[k] += A_PLUS;

            uint64_t post_fire_time = neurons_.last_spike_time[post_id];
            if (post_fire_time > 0) {
                uint64_t diff = ctx_.current_tick - post_fire_time;
                if (diff < static_cast<uint64_t>(STDP_WINDOW_MS)) {
                    synapses_.eligibility_traces[k] -= A_MINUS;
                }
            }
        }
    }
}

void BioEngine::inject_stimulus(uint32_t neuron_id, float current_mv) {
    if (neuron_id < neurons_.membrane_potential.size()) {
        neurons_.membrane_potential[neuron_id] += current_mv;
    }
}

void BioEngine::set_dopamine(float level) {
    ctx_.dopamine = level;
}

void BioEngine::set_acetylcholine(float level) {
    ctx_.acetylcholine = level;
}

void BioEngine::set_state(EngineState state) {
    ctx_.state = state;
}

} // namespace genesis