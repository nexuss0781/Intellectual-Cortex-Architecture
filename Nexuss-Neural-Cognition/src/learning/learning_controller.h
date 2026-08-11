#pragma once

#include "../types.h"
#include "../utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace genesis {

// Stage 1 structural flags. A zero-valued legacy flag remains active for
// backwards compatibility; explicit DISABLED always wins.
constexpr uint8_t LEARNING_SYNAPSE_FLAG_PRUNABLE = 0x02;
constexpr uint8_t LEARNING_SYNAPSE_FLAG_DISABLED = 0x04;

struct LearningSignal {
    float reward = 0.0f;
    float prediction_error = 0.0f;
    float novelty = 0.0f;
    float task_relevance = 0.0f;
    float executive_permission = 0.0f;
    uint64_t tick = 0;
};

struct LearningConfig {
    float eta = 0.001f;
    float trace_decay = 0.990050f;
    float a_plus = 0.10f;
    float a_minus = 0.12f;
    float timing_window_ms = 20.0f;
    float weight_min = 0.0001f;
    float weight_max = 1.0f;
    float modulation_min = -1.0f;
    float modulation_max = 1.0f;

    float reward_coefficient = 1.0f;
    float prediction_error_coefficient = 0.5f;
    float novelty_coefficient = 0.25f;
    float task_relevance_coefficient = 0.10f;
    float executive_permission_coefficient = 0.10f;

    float target_rate_hz = 50.0f;
    float homeostatic_rate = 0.01f;
    float homeostatic_gain_min = 0.25f;
    float homeostatic_gain_max = 4.0f;

    float prune_threshold = 0.0002f;
    uint32_t prune_patience_ticks = 100;
    uint32_t minimum_active_synapses = 1;

    bool learning_enabled = true;
    bool homeostasis_enabled = true;
    bool structural_enabled = true;
    bool normalization_enabled = false;
    float normalization_target = 1.0f;

    void validate() const {
        auto finite = [](float value) { return std::isfinite(value); };
        if (!finite(eta) || eta < 0.0f) throw std::invalid_argument("LearningConfig eta must be finite and non-negative");
        if (!finite(trace_decay) || trace_decay < 0.0f || trace_decay > 1.0f) throw std::invalid_argument("LearningConfig trace_decay must be in [0,1]");
        if (!finite(a_plus) || !finite(a_minus) || a_plus < 0.0f || a_minus < 0.0f) throw std::invalid_argument("LearningConfig STDP amplitudes must be finite and non-negative");
        if (!finite(timing_window_ms) || timing_window_ms < 0.0f) throw std::invalid_argument("LearningConfig timing window must be finite and non-negative");
        if (!finite(weight_min) || !finite(weight_max) || weight_min < 0.0f || weight_min >= weight_max) throw std::invalid_argument("LearningConfig weight bounds are invalid");
        if (!finite(modulation_min) || !finite(modulation_max) || modulation_min >= modulation_max) throw std::invalid_argument("LearningConfig modulation bounds are invalid");
        if (!finite(homeostatic_gain_min) || !finite(homeostatic_gain_max) || homeostatic_gain_min <= 0.0f || homeostatic_gain_min >= homeostatic_gain_max) throw std::invalid_argument("LearningConfig homeostatic gain bounds are invalid");
        if (!finite(prune_threshold) || prune_threshold < 0.0f || prune_patience_ticks == 0) throw std::invalid_argument("LearningConfig pruning parameters are invalid");
        if (!finite(normalization_target) || normalization_target <= 0.0f) throw std::invalid_argument("LearningConfig normalization target is invalid");
    }
};

struct StructuralProposal {
    uint32_t synapse_id = 0;
    uint32_t pre_id = 0;
    uint32_t post_id = 0;
    uint64_t proposed_tick = 0;
    std::string reason;
};

struct LearningMetrics {
    uint64_t pre_spike_events = 0;
    uint64_t post_spike_events = 0;
    uint64_t causal_events = 0;
    uint64_t anti_causal_events = 0;
    uint64_t weight_updates = 0;
    uint64_t clipped_updates = 0;
    uint64_t pruned_synapses = 0;
    uint64_t structural_proposals = 0;
    uint64_t bound_violations = 0;
    float last_modulation = 0.0f;
    float mean_firing_rate_hz = 0.0f;
    float homeostatic_gain = 1.0f;
    float min_weight = 0.0f;
    float max_weight = 0.0f;
    float mean_abs_trace = 0.0f;
};

class LearningController {
public:
    LearningController() { config_.validate(); }
    explicit LearningController(const LearningConfig& config) : config_(config) { config_.validate(); }

    void configure(const LearningConfig& config) {
        config.validate();
        config_ = config;
    }

    const LearningConfig& config() const { return config_; }

    void initialize(size_t neuron_count, size_t synapse_count) {
        post_last_tick_.assign(neuron_count, kNever);
        pre_last_tick_.assign(synapse_count, kNever);
        low_weight_age_.assign(synapse_count, 0);
        proposal_emitted_.assign(synapse_count, false);
        causal_gate_.assign(synapse_count, false);
        active_neuron_count_ = neuron_count;
        active_synapse_count_ = synapse_count;
        proposals_.clear();
        metrics_ = LearningMetrics{};
        homeostatic_gain_ = 1.0f;
        population_rate_hz_ = 0.0f;
    }

    void set_active_neuron_count(size_t count) { active_neuron_count_ = count; }
    void set_active_synapse_count(size_t count) { active_synapse_count_ = count; }

    void apply_modulation(const LearningSignal& signal) {
        signal_ = signal;
        const float raw = config_.reward_coefficient * signal.reward +
                          config_.prediction_error_coefficient * signal.prediction_error +
                          config_.novelty_coefficient * signal.novelty +
                          config_.task_relevance_coefficient * signal.task_relevance +
                          config_.executive_permission_coefficient * signal.executive_permission;
        modulation_ = utils::clip(raw, config_.modulation_min, config_.modulation_max);
        metrics_.last_modulation = modulation_;
    }

    float modulation() const { return modulation_; }
    const LearningSignal& signal() const { return signal_; }

    // Call once for each presynaptic event. Positive eligibility is generated
    // at the postsynaptic event, not merely because a presynaptic neuron fired.
    // This prevents every outgoing synapse from learning the same transition.
    void on_pre_spike(uint32_t synapse_id, SynapseBlock& synapses, uint64_t tick) {
        ensure_synapse_id(synapse_id, synapses);
        const uint32_t post_id = synapses.post_indices[synapse_id];
        if (post_id >= post_last_tick_.size()) {
            throw std::out_of_range("LearningController post neuron ID exceeds initialized population");
        }
        pre_last_tick_[synapse_id] = tick;
        float& trace = synapses.eligibility_traces[synapse_id];
        trace = utils::clip(trace + config_.a_plus, -1.0f, 1.0f);
        ++metrics_.pre_spike_events;

        const uint64_t last_post = post_last_tick_[post_id];
        if (last_post != kNever && tick >= last_post && tick - last_post <= static_cast<uint64_t>(config_.timing_window_ms)) {
            trace = utils::clip(trace - config_.a_minus, -1.0f, 1.0f);
            ++metrics_.anti_causal_events;
        }
    }

    void on_post_spike(uint32_t neuron_id, uint64_t tick) {
        if (neuron_id >= post_last_tick_.size()) {
            throw std::out_of_range("LearningController post neuron ID exceeds initialized population");
        }
        post_last_tick_[neuron_id] = tick;
        ++metrics_.post_spike_events;
    }

    void on_post_spike(uint32_t neuron_id, SynapseBlock& synapses, uint64_t tick) {
        if (neuron_id >= post_last_tick_.size()) {
            throw std::out_of_range("LearningController post neuron ID exceeds initialized population");
        }
        for (size_t sid = 0; sid < synapses.weights.size(); ++sid) {
            if (synapses.post_indices[sid] != neuron_id || sid >= pre_last_tick_.size()) continue;
            const uint64_t last_pre = pre_last_tick_[sid];
            if (last_pre != kNever && tick >= last_pre && tick - last_pre <= static_cast<uint64_t>(config_.timing_window_ms)) {
                float& trace = synapses.eligibility_traces[sid];
                trace = utils::clip(trace + config_.a_plus, -1.0f, 1.0f);
                causal_gate_[sid] = true;
                ++metrics_.causal_events;
            }
        }
        post_last_tick_[neuron_id] = tick;
        ++metrics_.post_spike_events;
    }

    template <typename IndexContainer>
    void on_post_spikes(const IndexContainer& fired, SynapseBlock& synapses, uint64_t tick) {
        for (uint32_t neuron_id : fired) on_post_spike(neuron_id, synapses, tick);
    }

    void observe_population_spikes(size_t fired_count, size_t active_neurons, float dt_ms = 1.0f) {
        if (active_neurons == 0 || !(dt_ms > 0.0f) || !std::isfinite(dt_ms)) return;
        active_neuron_count_ = active_neurons;
        const float observed_hz = static_cast<float>(fired_count) * (1000.0f / dt_ms) / static_cast<float>(active_neurons);
        population_rate_hz_ = 0.95f * population_rate_hz_ + 0.05f * observed_hz;
        if (config_.homeostasis_enabled) {
            const float error = config_.target_rate_hz - population_rate_hz_;
            homeostatic_gain_ = utils::clip(homeostatic_gain_ + config_.homeostatic_rate * error / std::max(1.0f, config_.target_rate_hz),
                                             config_.homeostatic_gain_min, config_.homeostatic_gain_max);
        }
        metrics_.mean_firing_rate_hz = population_rate_hz_;
        metrics_.homeostatic_gain = homeostatic_gain_;
    }

    // Decay eligibility and apply the bounded three-factor update.
    void update(SynapseBlock& synapses, const NeuronBlock& neurons, uint64_t tick) {
        const size_t count = std::min(active_synapse_count_, synapses.weights.size());
        double abs_trace_sum = 0.0;
        float min_weight = std::numeric_limits<float>::max();
        float max_weight = std::numeric_limits<float>::lowest();
        for (size_t sid = 0; sid < count; ++sid) {
            float& trace = synapses.eligibility_traces[sid];
            trace *= config_.trace_decay;
            if (!std::isfinite(trace)) {
                ++metrics_.bound_violations;
                trace = 0.0f;
            }
            abs_trace_sum += std::abs(trace);

            const bool disabled = sid < synapses.synapse_flags.size() &&
                                  (synapses.synapse_flags[sid] & LEARNING_SYNAPSE_FLAG_DISABLED) != 0;
            if (!disabled && config_.learning_enabled && modulation_ != 0.0f && causal_gate_[sid]) {
                const uint32_t post_id = synapses.post_indices[sid];
                float region_scale = 1.0f;
                if (post_id < neurons.plasticity_scale.size() && std::isfinite(neurons.plasticity_scale[post_id]) && neurons.plasticity_scale[post_id] > 0.0f) {
                    region_scale = neurons.plasticity_scale[post_id];
                }
                float precision = 1.0f;
                if (sid < synapses.precision_scale.size() && std::isfinite(synapses.precision_scale[sid])) {
                    precision = utils::clip(synapses.precision_scale[sid], 0.0f, 1.0f);
                }
                const float eta = config_.eta * region_scale * homeostatic_gain_;
                const float delta = eta * trace * modulation_ * precision;
                const float old_weight = synapses.weights[sid];
                const float unclipped = old_weight + delta;
                const float new_weight = utils::clip(unclipped, config_.weight_min, config_.weight_max);
                synapses.weights[sid] = new_weight;
                ++metrics_.weight_updates;
                if (new_weight != unclipped) ++metrics_.clipped_updates;
            }

            if (!std::isfinite(synapses.weights[sid]) ||
                synapses.weights[sid] < config_.weight_min || synapses.weights[sid] > config_.weight_max) {
                ++metrics_.bound_violations;
                synapses.weights[sid] = utils::clip(synapses.weights[sid], config_.weight_min, config_.weight_max);
            }
            min_weight = std::min(min_weight, synapses.weights[sid]);
            max_weight = std::max(max_weight, synapses.weights[sid]);

            const bool prunable = sid < synapses.synapse_flags.size() &&
                                  (synapses.synapse_flags[sid] & LEARNING_SYNAPSE_FLAG_PRUNABLE) != 0;
            if (config_.structural_enabled && prunable &&
                std::abs(synapses.weights[sid]) <= config_.prune_threshold) {
                if (low_weight_age_[sid] < std::numeric_limits<uint32_t>::max()) ++low_weight_age_[sid];
                if (low_weight_age_[sid] >= config_.prune_patience_ticks && !proposal_emitted_[sid]) {
                    proposal_emitted_[sid] = true;
                    proposals_.push_back({static_cast<uint32_t>(sid), synapses.pre_indices[sid], synapses.post_indices[sid], tick, "low_weight_patience"});
                    ++metrics_.structural_proposals;
                }
            } else if (sid < low_weight_age_.size()) {
                low_weight_age_[sid] = 0;
                if (sid < proposal_emitted_.size()) proposal_emitted_[sid] = false;
            }
        }
        if (config_.normalization_enabled) normalize_weights(synapses, count);
        if (count > 0) {
            metrics_.min_weight = min_weight;
            metrics_.max_weight = max_weight;
            metrics_.mean_abs_trace = static_cast<float>(abs_trace_sum / static_cast<double>(count));
        }
    }

    // Structural change is explicit and auditable. IDs are never compacted in
    // Stage 1; pruning marks a stable slot disabled for later allocation work.
    size_t apply_structural_proposals(SynapseBlock& synapses) {
        size_t applied = 0;
        for (const StructuralProposal& proposal : proposals_) {
            if (proposal.synapse_id >= synapses.weights.size()) continue;
            if (proposal.synapse_id < synapses.synapse_flags.size()) {
                const uint8_t before = synapses.synapse_flags[proposal.synapse_id];
                synapses.synapse_flags[proposal.synapse_id] |= LEARNING_SYNAPSE_FLAG_DISABLED;
                if ((before & LEARNING_SYNAPSE_FLAG_DISABLED) == 0) {
                    ++applied;
                    ++metrics_.pruned_synapses;
                }
            }
        }
        proposals_.clear();
        return applied;
    }

    const std::vector<StructuralProposal>& structural_proposals() const { return proposals_; }
    float homeostatic_gain() const { return homeostatic_gain_; }
    float population_rate_hz() const { return population_rate_hz_; }
    const LearningMetrics& metrics() const { return metrics_; }

    void reset_traces(SynapseBlock& synapses) {
        std::fill(synapses.eligibility_traces.begin(), synapses.eligibility_traces.end(), 0.0f);
        std::fill(post_last_tick_.begin(), post_last_tick_.end(), kNever);
        std::fill(pre_last_tick_.begin(), pre_last_tick_.end(), kNever);
        std::fill(causal_gate_.begin(), causal_gate_.end(), false);
    }

    bool is_synapse_disabled(const SynapseBlock& synapses, size_t sid) const {
        return sid < synapses.synapse_flags.size() &&
               (synapses.synapse_flags[sid] & LEARNING_SYNAPSE_FLAG_DISABLED) != 0;
    }

private:
    static constexpr uint64_t kNever = std::numeric_limits<uint64_t>::max();

    void normalize_weights(SynapseBlock& synapses, size_t count) {
        uint32_t max_pre = 0;
        for (size_t sid = 0; sid < count; ++sid) max_pre = std::max(max_pre, synapses.pre_indices[sid]);
        std::vector<float> totals(static_cast<size_t>(max_pre) + 1, 0.0f);
        for (size_t sid = 0; sid < count; ++sid) {
            if (!is_synapse_disabled(synapses, sid)) totals[synapses.pre_indices[sid]] += synapses.weights[sid];
        }
        for (size_t sid = 0; sid < count; ++sid) {
            const float total = totals[synapses.pre_indices[sid]];
            if (!is_synapse_disabled(synapses, sid) && total > config_.normalization_target) {
                synapses.weights[sid] = utils::clip(
                    synapses.weights[sid] * config_.normalization_target / total,
                    config_.weight_min, config_.weight_max);
            }
        }
    }

    void ensure_synapse_id(uint32_t synapse_id, const SynapseBlock& synapses) const {
        if (synapse_id >= synapses.weights.size() || synapse_id >= synapses.pre_indices.size() ||
            synapse_id >= synapses.post_indices.size() || synapse_id >= synapses.eligibility_traces.size()) {
            throw std::out_of_range("LearningController synapse ID exceeds SynapseBlock");
        }
        if (synapse_id >= low_weight_age_.size()) {
            throw std::logic_error("LearningController was not initialized for this synapse population");
        }
    }

    LearningConfig config_;
    LearningSignal signal_;
    float modulation_ = 0.0f;
    float homeostatic_gain_ = 1.0f;
    float population_rate_hz_ = 0.0f;
    size_t active_neuron_count_ = 0;
    size_t active_synapse_count_ = 0;
    std::vector<uint64_t> post_last_tick_;
    std::vector<uint64_t> pre_last_tick_;
    std::vector<uint32_t> low_weight_age_;
    std::vector<bool> proposal_emitted_;
    std::vector<bool> causal_gate_;
    std::vector<StructuralProposal> proposals_;
    LearningMetrics metrics_;
};

} // namespace genesis
