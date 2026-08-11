#pragma once

#include "memory_system.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace genesis {

struct SparseCode {
    std::vector<float> values;

    explicit SparseCode(size_t dimensions = 0, float fill = 0.0f) : values(dimensions, fill) {}
    explicit SparseCode(std::vector<float> input) : values(std::move(input)) {}

    size_t dimensions() const { return values.size(); }
    float norm_squared() const {
        float total = 0.0f;
        for (float value : values) total += value * value;
        return total;
    }
    uint64_t signature() const {
        uint64_t hash = 1469598103934665603ULL;
        for (float value : values) {
            const int32_t quantized = static_cast<int32_t>(std::lround(std::max(-8.0f, std::min(8.0f, value)) * 1024.0f));
            const auto* bytes = reinterpret_cast<const uint8_t*>(&quantized);
            for (size_t i = 0; i < sizeof(quantized); ++i) { hash ^= bytes[i]; hash *= 1099511628211ULL; }
        }
        return hash;
    }
};

struct CognitiveContext {
    uint64_t context_id = 0;
    uint64_t goal_id = 0;
    uint64_t memory_pointer_id = 0;
    float goal_strength = 0.0f;
    std::vector<float> goal_code;
};

struct PredictionProposal {
    uint64_t proposal_id = 0;
    uint64_t source_population = 0;
    uint64_t context_id = 0;
    SparseCode prediction;
    SparseCode observation;
    float normalized_error = 0.0f;
    float precision = 1.0f;
    float precision_weighted_error = 0.0f;
    float surprise = 0.0f;
    float uncertainty = 0.0f;
    float goal_bias = 0.0f;
    float expected_information_gain = 0.0f;
    float cost = 0.0f;
    float salience = 0.0f;
    float confidence = 0.0f;
};

struct WorkspaceBroadcast {
    uint64_t broadcast_id = 0;
    uint64_t tick = 0;
    uint32_t winner_count = 0;
    float coalition_score = 0.0f;
    float ignition_margin = 0.0f;
    uint64_t goal_id = 0;
    std::vector<uint64_t> proposal_ids;
};

struct WorkspaceState {
    bool ignited = false;
    uint64_t last_tick = 0;
    uint64_t ignition_count = 0;
    uint32_t pending_count = 0;
    uint64_t broadcast_count = 0;
};

struct PredictiveConfig {
    size_t dimensions = 8;
    float scale_learning_rate = 0.05f;
    float transition_learning_rate = 0.20f;
    float precision_learning_rate = 0.05f;
    float precision_min = 0.10f;
    float precision_max = 10.0f;
    float initial_scale = 1.0f;
    float initial_precision = 1.0f;
    float epsilon = 1e-5f;
    float novelty_weight = 0.25f;
    float goal_weight = 1.0f;
    float uncertainty_weight = 0.25f;
    float information_gain_weight = 0.50f;
    float cost_weight = 0.10f;
    bool learning_enabled = true;
};

struct WorkspaceConfig {
    size_t capacity = 3;
    float ignition_threshold = 0.75f;
    float minimum_coherence = 0.25f;
    uint32_t ignition_consecutive_ticks = 2;
    float hysteresis = 0.15f;
    size_t broadcast_budget_bytes = 4096;
};

class PredictivePopulation {
public:
    PredictivePopulation(uint64_t population_id = 1, const PredictiveConfig& config = PredictiveConfig{})
        : population_id_(population_id), config_(config), scale_(config.dimensions, config.initial_scale),
          default_prediction_(config.dimensions, 0.0f), previous_observation_(config.dimensions, 0.0f),
          pending_observation_(config.dimensions, 0.0f), precision_(config.initial_precision) {
        validate_config();
    }

    PredictionProposal predict(const CognitiveContext& context) const {
        const SparseCode prediction = lookup_prediction(previous_observation_.signature(), context.context_id);
        return make_proposal(context, prediction, previous_observation_);
    }

    void observe(const SparseCode& observation) {
        validate_code(observation);
        pending_observation_ = observation;
        const SparseCode prediction = lookup_prediction(previous_observation_.signature(), last_context_.context_id);
        current_proposal_ = make_proposal(last_context_, prediction, observation);
        for (size_t i = 0; i < config_.dimensions; ++i) {
            const float absolute_error = std::abs(observation.values[i] - prediction.values[i]);
            scale_[i] = std::max(config_.epsilon, (1.0f - config_.scale_learning_rate) * scale_[i] + config_.scale_learning_rate * absolute_error);
        }
        has_current_proposal_ = true;
        last_observation_error_ = current_proposal_.normalized_error;
    }

    void learn(const LearningSignal& signal) {
        if (!has_current_proposal_) return;
        if (config_.learning_enabled && signal.executive_permission != 0.0f) {
            const uint64_t key = transition_key(previous_observation_.signature(), last_context_.context_id);
            auto& transition = transitions_[key];
            if (transition.values.empty()) transition = pending_observation_;
            for (size_t i = 0; i < config_.dimensions; ++i) transition.values[i] = (1.0f - config_.transition_learning_rate) * transition.values[i] + config_.transition_learning_rate * pending_observation_.values[i];
            for (size_t i = 0; i < config_.dimensions; ++i) default_prediction_.values[i] = (1.0f - config_.transition_learning_rate * 0.5f) * default_prediction_.values[i] + config_.transition_learning_rate * 0.5f * pending_observation_.values[i];
        }
        previous_observation_ = pending_observation_;
        has_current_proposal_ = false;
    }

    PredictionProposal step(const CognitiveContext& context, const SparseCode& observation, const LearningSignal& signal = LearningSignal{}) {
        last_context_ = context;
        observe(observation);
        learn(signal);
        return current_proposal_;
    }

    void set_precision(float precision) {
        if (!std::isfinite(precision)) throw std::invalid_argument("precision must be finite");
        precision_ = clip(precision, config_.precision_min, config_.precision_max);
    }

    void update_precision(float target) {
        if (!std::isfinite(target)) throw std::invalid_argument("precision target must be finite");
        precision_ = clip(precision_ + config_.precision_learning_rate * (target - precision_), config_.precision_min, config_.precision_max);
    }

    uint64_t population_id() const { return population_id_; }
    float precision() const { return precision_; }
    float last_normalized_error() const { return last_observation_error_; }
    const PredictionProposal& current_proposal() const { return current_proposal_; }
    const std::vector<float>& scale() const { return scale_; }

    uint64_t state_hash() const {
        uint64_t hash = 1469598103934665603ULL;
        auto mix = [&hash](const void* data, size_t bytes) { const auto* p = static_cast<const uint8_t*>(data); for (size_t i = 0; i < bytes; ++i) { hash ^= p[i]; hash *= 1099511628211ULL; } };
        mix(&population_id_, sizeof(population_id_)); mix(&precision_, sizeof(precision_)); mix(&last_observation_error_, sizeof(last_observation_error_)); mix(&next_proposal_id_, sizeof(next_proposal_id_));
        mix(&last_context_.context_id, sizeof(last_context_.context_id)); mix(&last_context_.goal_id, sizeof(last_context_.goal_id)); mix(&last_context_.memory_pointer_id, sizeof(last_context_.memory_pointer_id)); mix(&last_context_.goal_strength, sizeof(last_context_.goal_strength));
        for (float value : last_context_.goal_code) mix(&value, sizeof(value));
        for (float value : scale_) mix(&value, sizeof(value));
        for (float value : default_prediction_.values) mix(&value, sizeof(value));
        for (float value : previous_observation_.values) mix(&value, sizeof(value));
        for (float value : pending_observation_.values) mix(&value, sizeof(value));
        const uint8_t active = has_current_proposal_ ? 1U : 0U; mix(&active, sizeof(active));
        for (const auto& item : transitions_) { mix(&item.first, sizeof(item.first)); for (float value : item.second.values) mix(&value, sizeof(value)); }
        return hash;
    }

    void serialize(std::ostream& output) const {
        write(output, population_id_); write(output, precision_); write(output, last_observation_error_); write(output, next_proposal_id_);
        write(output, last_context_.context_id); write(output, last_context_.goal_id); write(output, last_context_.memory_pointer_id); write(output, last_context_.goal_strength); write_vector(output, last_context_.goal_code);
        write_vector(output, scale_); write_vector(output, default_prediction_.values); write_vector(output, previous_observation_.values); write_vector(output, pending_observation_.values);
        const uint8_t active = has_current_proposal_ ? 1U : 0U; write(output, active);
        write(output, static_cast<uint64_t>(transitions_.size()));
        for (const auto& item : transitions_) { write(output, item.first); write_vector(output, item.second.values); }
    }

    void deserialize(std::istream& input) {
        read(input, population_id_); read(input, precision_); read(input, last_observation_error_); read(input, next_proposal_id_);
        read(input, last_context_.context_id); read(input, last_context_.goal_id); read(input, last_context_.memory_pointer_id); read(input, last_context_.goal_strength); read_vector(input, last_context_.goal_code);
        read_vector(input, scale_); read_vector(input, default_prediction_.values); read_vector(input, previous_observation_.values); read_vector(input, pending_observation_.values);
        uint8_t active = 0; read(input, active); has_current_proposal_ = active != 0;
        if ((last_context_.goal_code.size() != 0 && last_context_.goal_code.size() != config_.dimensions) || scale_.size() != config_.dimensions || default_prediction_.values.size() != config_.dimensions || previous_observation_.values.size() != config_.dimensions || pending_observation_.values.size() != config_.dimensions) throw std::runtime_error("predictive state dimension mismatch");
        uint64_t count = 0; read(input, count); if (count > 1000000) throw std::runtime_error("predictive transition count is too large"); transitions_.clear();
        for (uint64_t i = 0; i < count; ++i) { uint64_t key = 0; std::vector<float> values; read(input, key); read_vector(input, values); if (values.size() != config_.dimensions) throw std::runtime_error("transition state dimension mismatch"); transitions_[key] = SparseCode(std::move(values)); }
        if (!std::isfinite(precision_) || precision_ < config_.precision_min || precision_ > config_.precision_max) throw std::runtime_error("persisted precision is invalid");
    }

private:
    static float clip(float value, float low, float high) { return std::max(low, std::min(high, value)); }

    static void write_raw(std::ostream& output, const void* data, size_t bytes) { output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(bytes)); if (!output) throw std::runtime_error("predictive state write failed"); }
    static void read_raw(std::istream& input, void* data, size_t bytes) { input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(bytes)); if (!input) throw std::runtime_error("predictive state read failed"); }
    template <typename T> static void write(std::ostream& output, const T& value) { write_raw(output, &value, sizeof(T)); }
    template <typename T> static void read(std::istream& input, T& value) { read_raw(input, &value, sizeof(T)); }
    static void write_vector(std::ostream& output, const std::vector<float>& values) { write(output, static_cast<uint64_t>(values.size())); if (!values.empty()) write_raw(output, values.data(), values.size() * sizeof(float)); }
    static void read_vector(std::istream& input, std::vector<float>& values) { uint64_t size = 0; read(input, size); if (size > 1000000) throw std::runtime_error("persisted vector is too large"); values.resize(static_cast<size_t>(size)); if (!values.empty()) read_raw(input, values.data(), values.size() * sizeof(float)); }

    void validate_config() const {
        if (config_.dimensions == 0 || config_.precision_min <= 0.0f || config_.precision_max < config_.precision_min || config_.initial_scale <= 0.0f) throw std::invalid_argument("invalid predictive configuration");
    }
    void validate_code(const SparseCode& code) const { if (code.values.size() != config_.dimensions) throw std::invalid_argument("prediction code dimension mismatch"); for (float value : code.values) if (!std::isfinite(value)) throw std::invalid_argument("prediction code contains non-finite value"); }
    uint64_t transition_key(uint64_t observation_signature, uint64_t context_id) const { return observation_signature ^ (context_id + 0x9e3779b97f4a7c15ULL + (observation_signature << 6) + (observation_signature >> 2)); }
    SparseCode lookup_prediction(uint64_t observation_signature, uint64_t context_id) const { const auto iterator = transitions_.find(transition_key(observation_signature, context_id)); return iterator == transitions_.end() ? default_prediction_ : iterator->second; }

    PredictionProposal make_proposal(const CognitiveContext& context, const SparseCode& prediction, const SparseCode& observation) const {
        validate_code(prediction); validate_code(observation);
        PredictionProposal proposal;
        proposal.proposal_id = next_proposal_id_++;
        proposal.source_population = population_id_;
        proposal.context_id = context.context_id;
        proposal.prediction = prediction;
        proposal.observation = observation;
        float normalized_squared = 0.0f;
        float raw_squared = 0.0f;
        for (size_t i = 0; i < config_.dimensions; ++i) {
            const float error = observation.values[i] - prediction.values[i];
            raw_squared += error * error;
            normalized_squared += (error / (scale_[i] + config_.epsilon)) * (error / (scale_[i] + config_.epsilon));
        }
        proposal.normalized_error = std::sqrt(normalized_squared / static_cast<float>(config_.dimensions));
        proposal.precision = precision_;
        proposal.precision_weighted_error = precision_ * proposal.normalized_error;
        proposal.surprise = raw_squared / static_cast<float>(config_.dimensions) + config_.novelty_weight * proposal.normalized_error;
        float goal_similarity = 0.0f;
        if (!context.goal_code.empty()) {
            if (context.goal_code.size() != observation.values.size()) throw std::invalid_argument("goal code dimension mismatch");
            float dot = 0.0f, goal_norm = 0.0f, obs_norm = 0.0f;
            for (size_t i = 0; i < observation.values.size(); ++i) { dot += observation.values[i] * context.goal_code[i]; goal_norm += context.goal_code[i] * context.goal_code[i]; obs_norm += observation.values[i] * observation.values[i]; }
            if (goal_norm > 0.0f && obs_norm > 0.0f) goal_similarity = dot / std::sqrt(goal_norm * obs_norm);
        }
        proposal.goal_bias = context.goal_strength * goal_similarity;
        proposal.uncertainty = 1.0f / (1.0f + std::accumulate(scale_.begin(), scale_.end(), 0.0f) / static_cast<float>(scale_.size()));
        proposal.expected_information_gain = proposal.precision_weighted_error * (0.5f + proposal.uncertainty);
        proposal.cost = static_cast<float>(config_.dimensions) / 100.0f;
        proposal.confidence = 1.0f / (1.0f + proposal.precision_weighted_error);
        proposal.salience = score(proposal);
        return proposal;
    }

public:
    static float score(const PredictionProposal& proposal) {
        return 1.0f * proposal.surprise + 1.0f * proposal.goal_bias + 0.25f * proposal.uncertainty + 0.50f * proposal.expected_information_gain - 0.10f * proposal.cost;
    }

private:
    uint64_t population_id_ = 1;
    PredictiveConfig config_;
    std::vector<float> scale_;
    SparseCode default_prediction_;
    SparseCode previous_observation_;
    SparseCode pending_observation_;
    CognitiveContext last_context_;
    PredictionProposal current_proposal_;
    bool has_current_proposal_ = false;
    float precision_ = 1.0f;
    float last_observation_error_ = 0.0f;
    mutable uint64_t next_proposal_id_ = 1;
    std::map<uint64_t, SparseCode> transitions_;
};

class GlobalWorkspace {
public:
    explicit GlobalWorkspace(const WorkspaceConfig& config = WorkspaceConfig{}) : config_(config) {
        if (config_.capacity == 0 || config_.broadcast_budget_bytes < sizeof(WorkspaceBroadcast)) throw std::invalid_argument("invalid workspace configuration");
    }

    void submit(PredictionProposal proposal) {
        if (!std::isfinite(proposal.salience)) throw std::invalid_argument("workspace proposal salience is non-finite");
        pending_.push_back(std::move(proposal));
    }

    std::optional<WorkspaceBroadcast> step(uint64_t tick, uint64_t goal_id = 0) {
        state_.last_tick = tick;
        state_.pending_count = static_cast<uint32_t>(pending_.size());
        std::stable_sort(pending_.begin(), pending_.end(), [](const PredictionProposal& a, const PredictionProposal& b) {
            if (a.salience != b.salience) return a.salience > b.salience;
            return a.proposal_id < b.proposal_id;
        });
        const size_t winner_count = std::min(config_.capacity, pending_.size());
        float coalition_score = 0.0f;
        for (size_t i = 0; i < winner_count; ++i) coalition_score += pending_[i].salience;
        if (winner_count > 0) coalition_score /= static_cast<float>(winner_count);
        float coherence = 0.0f;
        if (winner_count > 0) {
            float variance = 0.0f;
            for (size_t i = 0; i < winner_count; ++i) { const float delta = pending_[i].salience - coalition_score; variance += delta * delta; }
            variance /= static_cast<float>(winner_count);
            coherence = std::max(0.0f, 1.0f - variance / (1.0f + coalition_score * coalition_score));
        }
        const bool candidate = winner_count > 0 && coalition_score >= config_.ignition_threshold && coherence >= config_.minimum_coherence;
        if (candidate) ++consecutive_candidate_ticks_; else consecutive_candidate_ticks_ = 0;
        if (!state_.ignited && consecutive_candidate_ticks_ >= config_.ignition_consecutive_ticks) state_.ignited = true;
        if (state_.ignited && (!candidate && coalition_score < config_.ignition_threshold - config_.hysteresis)) state_.ignited = false;

        std::optional<WorkspaceBroadcast> broadcast;
        if (state_.ignited && winner_count > 0) {
            WorkspaceBroadcast item;
            item.broadcast_id = next_broadcast_id_++;
            item.tick = tick;
            item.winner_count = static_cast<uint32_t>(winner_count);
            item.coalition_score = coalition_score;
            item.ignition_margin = coalition_score - config_.ignition_threshold;
            item.goal_id = goal_id;
            for (size_t i = 0; i < winner_count; ++i) item.proposal_ids.push_back(pending_[i].proposal_id);
            const size_t bytes = sizeof(WorkspaceBroadcast) + item.proposal_ids.size() * sizeof(uint64_t);
            if (bytes > config_.broadcast_budget_bytes) throw std::runtime_error("workspace broadcast budget exceeded");
            ++state_.broadcast_count;
            ++state_.ignition_count;
            broadcast = item;
        }
        pending_.clear();
        state_.pending_count = 0;
        return broadcast;
    }

    WorkspaceState state() const { return state_; }
    const WorkspaceConfig& config() const { return config_; }
    size_t pending_size() const { return pending_.size(); }

    uint64_t state_hash() const {
        uint64_t hash = 1469598103934665603ULL;
        const auto mix = [&hash](const void* data, size_t bytes) { const auto* p = static_cast<const uint8_t*>(data); for (size_t i = 0; i < bytes; ++i) { hash ^= p[i]; hash *= 1099511628211ULL; } };
        const uint64_t capacity = static_cast<uint64_t>(config_.capacity); const uint64_t broadcast_budget = static_cast<uint64_t>(config_.broadcast_budget_bytes);
        mix(&capacity, sizeof(capacity)); mix(&config_.ignition_threshold, sizeof(config_.ignition_threshold)); mix(&config_.minimum_coherence, sizeof(config_.minimum_coherence)); mix(&config_.ignition_consecutive_ticks, sizeof(config_.ignition_consecutive_ticks)); mix(&config_.hysteresis, sizeof(config_.hysteresis)); mix(&broadcast_budget, sizeof(broadcast_budget));
        const uint8_t ignited = state_.ignited ? 1U : 0U; mix(&ignited, sizeof(ignited)); mix(&state_.last_tick, sizeof(state_.last_tick)); mix(&state_.ignition_count, sizeof(state_.ignition_count)); mix(&state_.pending_count, sizeof(state_.pending_count)); mix(&state_.broadcast_count, sizeof(state_.broadcast_count));
        mix(&next_broadcast_id_, sizeof(next_broadcast_id_)); mix(&consecutive_candidate_ticks_, sizeof(consecutive_candidate_ticks_));
        return hash;
    }

    void serialize(std::ostream& output) const {
        const auto write = [&output](const auto& value) { output.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
        const uint64_t capacity = static_cast<uint64_t>(config_.capacity);
        const uint64_t broadcast_budget = static_cast<uint64_t>(config_.broadcast_budget_bytes);
        const uint8_t ignited = state_.ignited ? 1U : 0U;
        write(capacity); write(config_.ignition_threshold); write(config_.minimum_coherence); write(config_.ignition_consecutive_ticks); write(config_.hysteresis); write(broadcast_budget);
        write(ignited); write(state_.last_tick); write(state_.ignition_count); write(state_.pending_count); write(state_.broadcast_count);
        write(next_broadcast_id_); write(consecutive_candidate_ticks_);
        if (!output) throw std::runtime_error("workspace state write failed");
    }

    void deserialize(std::istream& input) {
        const auto read = [&input](auto& value) { input.read(reinterpret_cast<char*>(&value), sizeof(value)); };
        WorkspaceConfig loaded_config;
        WorkspaceState loaded_state;
        uint64_t capacity = 0, broadcast_budget = 0;
        uint8_t ignited = 0;
        uint64_t loaded_next_broadcast_id = 0;
        uint32_t loaded_consecutive_candidate_ticks = 0;
        read(capacity); read(loaded_config.ignition_threshold); read(loaded_config.minimum_coherence); read(loaded_config.ignition_consecutive_ticks); read(loaded_config.hysteresis); read(broadcast_budget);
        read(ignited); read(loaded_state.last_tick); read(loaded_state.ignition_count); read(loaded_state.pending_count); read(loaded_state.broadcast_count);
        read(loaded_next_broadcast_id); read(loaded_consecutive_candidate_ticks);
        if (!input || capacity == 0 || capacity > 1000000 || broadcast_budget < sizeof(WorkspaceBroadcast) || broadcast_budget > (1ULL << 32) || ignited > 1U || loaded_next_broadcast_id == 0 || loaded_config.ignition_consecutive_ticks == 0 || !std::isfinite(loaded_config.ignition_threshold) || !std::isfinite(loaded_config.minimum_coherence) || !std::isfinite(loaded_config.hysteresis) || loaded_config.ignition_threshold < 0.0f || loaded_config.minimum_coherence < 0.0f || loaded_config.hysteresis < 0.0f) throw std::runtime_error("workspace state is invalid");
        loaded_config.capacity = static_cast<size_t>(capacity);
        loaded_config.broadcast_budget_bytes = static_cast<size_t>(broadcast_budget);
        loaded_state.ignited = ignited != 0;
        config_ = loaded_config;
        state_ = loaded_state;
        next_broadcast_id_ = loaded_next_broadcast_id;
        consecutive_candidate_ticks_ = loaded_consecutive_candidate_ticks;
    }

private:
    WorkspaceConfig config_;
    WorkspaceState state_;
    uint64_t next_broadcast_id_ = 1;
    uint32_t consecutive_candidate_ticks_ = 0;
    std::vector<PredictionProposal> pending_;
};

class PredictiveWorkspace {
public:
    PredictiveWorkspace(const PredictiveConfig& population_config = PredictiveConfig{}, const WorkspaceConfig& workspace_config = WorkspaceConfig{})
        : population_config_(population_config), workspace_(workspace_config) {}

    PredictivePopulation& add_population(uint64_t population_id) {
        populations_.emplace_back(population_id, population_config_);
        return populations_.back();
    }

    PredictivePopulation& population(size_t index) { return populations_.at(index); }
    const PredictivePopulation& population(size_t index) const { return populations_.at(index); }
    size_t population_count() const { return populations_.size(); }
    GlobalWorkspace& workspace() { return workspace_; }
    const GlobalWorkspace& workspace() const { return workspace_; }

    void set_consolidation_candidates(const std::vector<ConsolidationCandidate>& candidates) { candidates_ = candidates; }
    const std::vector<ConsolidationCandidate>& consolidation_candidates() const { return candidates_; }

    std::optional<WorkspaceBroadcast> cycle(const CognitiveContext& context, const std::vector<SparseCode>& observations, uint64_t tick, const LearningSignal& signal = LearningSignal{}) {
        if (observations.size() != populations_.size()) throw std::invalid_argument("workspace observation count mismatch");
        for (size_t i = 0; i < populations_.size(); ++i) workspace_.submit(populations_[i].step(context, observations[i], signal));
        return workspace_.step(tick, context.goal_id);
    }

    uint64_t state_hash() const {
        uint64_t hash = workspace_.state_hash();
        const uint64_t population_count = populations_.size(); const uint64_t candidate_count = candidates_.size();
        hash ^= population_count + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        hash ^= candidate_count + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        for (const auto& population : populations_) { const uint64_t value = population.state_hash(); hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2); }
        for (const auto& candidate : candidates_) {
            hash ^= candidate.pointer_id + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
            hash ^= candidate.context_id + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
            hash ^= static_cast<uint64_t>(candidate.relation_type) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
            hash ^= static_cast<uint64_t>(std::lround(candidate.evidence * 1000000.0f)) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
            hash ^= static_cast<uint64_t>(std::lround(candidate.confidence * 1000000.0f)) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
            hash ^= candidate.supporting_episode_count + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

    void save(const std::filesystem::path& path) const {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        const uint32_t magic = 0x33575350U;
        const uint32_t version = 1;
        output.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        output.write(reinterpret_cast<const char*>(&version), sizeof(version));
        const uint64_t count = populations_.size();
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& population : populations_) population.serialize(output);
        workspace_.serialize(output);
        const uint64_t candidate_count = candidates_.size();
        output.write(reinterpret_cast<const char*>(&candidate_count), sizeof(candidate_count));
        for (const auto& candidate : candidates_) {
            output.write(reinterpret_cast<const char*>(&candidate.pointer_id), sizeof(candidate.pointer_id));
            output.write(reinterpret_cast<const char*>(&candidate.context_id), sizeof(candidate.context_id));
            output.write(reinterpret_cast<const char*>(&candidate.relation_type), sizeof(candidate.relation_type));
            output.write(reinterpret_cast<const char*>(&candidate.evidence), sizeof(candidate.evidence));
            output.write(reinterpret_cast<const char*>(&candidate.confidence), sizeof(candidate.confidence));
            output.write(reinterpret_cast<const char*>(&candidate.supporting_episode_count), sizeof(candidate.supporting_episode_count));
        }
        if (!output) throw std::runtime_error("predictive workspace save failed");
    }

    void load(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("predictive workspace state cannot be opened");
        uint32_t magic = 0, version = 0; uint64_t count = 0;
        input.read(reinterpret_cast<char*>(&magic), sizeof(magic)); input.read(reinterpret_cast<char*>(&version), sizeof(version)); input.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (!input || magic != 0x33575350U || version != 1 || count > 10000) throw std::runtime_error("predictive workspace header is invalid");
        std::vector<PredictivePopulation> loaded;
        loaded.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) { loaded.emplace_back(i + 1, population_config_); loaded.back().deserialize(input); }
        GlobalWorkspace loaded_workspace(workspace_.config());
        loaded_workspace.deserialize(input);
        uint64_t candidate_count = 0; input.read(reinterpret_cast<char*>(&candidate_count), sizeof(candidate_count));
        if (!input || candidate_count > 100000) throw std::runtime_error("predictive workspace candidate section is invalid");
        std::vector<ConsolidationCandidate> loaded_candidates(static_cast<size_t>(candidate_count));
        for (auto& candidate : loaded_candidates) {
            input.read(reinterpret_cast<char*>(&candidate.pointer_id), sizeof(candidate.pointer_id));
            input.read(reinterpret_cast<char*>(&candidate.context_id), sizeof(candidate.context_id));
            input.read(reinterpret_cast<char*>(&candidate.relation_type), sizeof(candidate.relation_type));
            input.read(reinterpret_cast<char*>(&candidate.evidence), sizeof(candidate.evidence));
            input.read(reinterpret_cast<char*>(&candidate.confidence), sizeof(candidate.confidence));
            input.read(reinterpret_cast<char*>(&candidate.supporting_episode_count), sizeof(candidate.supporting_episode_count));
            if (!std::isfinite(candidate.evidence) || !std::isfinite(candidate.confidence)) throw std::runtime_error("predictive workspace candidate contains non-finite values");
        }
        if (!input) throw std::runtime_error("predictive workspace state is truncated");
        populations_.swap(loaded); workspace_ = std::move(loaded_workspace); candidates_.swap(loaded_candidates);
    }

private:
    PredictiveConfig population_config_;
    std::vector<PredictivePopulation> populations_;
    GlobalWorkspace workspace_;
    std::vector<ConsolidationCandidate> candidates_;
};

} // namespace genesis
