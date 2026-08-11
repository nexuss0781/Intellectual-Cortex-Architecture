#pragma once

#include "learning_controller.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace genesis {

class MemoryError : public std::runtime_error {
public:
    explicit MemoryError(const std::string& message) : std::runtime_error(message) {}
};

struct EventHeader {
    uint64_t event_id = 0;
    uint64_t tick = 0;
    uint32_t source_module = 0;
    uint32_t event_type = 0;
    uint64_t payload_id = 0;
    uint64_t provenance_id = 0;
};

struct MemoryEvent {
    EventHeader header;
    uint64_t context_hash = 0;
    float value = 0.0f;
};

struct EpisodeRecord {
    uint64_t episode_id = 0;
    uint64_t start_tick = 0;
    uint64_t end_tick = 0;
    uint64_t context_pointer_id = 0;
    uint64_t event_offset = 0;
    uint32_t event_count = 0;
    float novelty = 0.0f;
    float prediction_error = 0.0f;
    float reward = 0.0f;
    float future_utility = 0.0f;
    uint32_t checksum = 0;
};

struct ReplayIndex {
    uint64_t episode_id = 0;
    uint64_t cue_hash = 0;
    uint64_t context_hash = 0;
    float retrieval_strength = 0.0f;
    float replay_priority = 0.0f;
    uint32_t access_count = 0;
    uint32_t flags = 0;
};

struct ReplayQuery {
    uint64_t cue_hash = 0;
    uint64_t context_hash = 0;
    uint64_t current_tick = 0;
    uint64_t max_age_ticks = std::numeric_limits<uint64_t>::max();
    float minimum_priority = -std::numeric_limits<float>::infinity();
};

enum class ReplayMode : uint32_t {
    ExactEvent = 0,
    CompressedPointer = 1,
    GenerativeReconstruction = 2
};

struct ReplayReport {
    uint32_t selected_episodes = 0;
    uint32_t replayed_events = 0;
    uint32_t pointer_replays = 0;
    uint32_t generated_replays = 0;
    uint32_t checksum_failures = 0;
    float mean_reconstruction_error = 0.0f;
    uint64_t budget_ticks = 0;
};

struct ConsolidationCandidate {
    uint64_t pointer_id = 0;
    uint64_t context_id = 0;
    uint32_t relation_type = 0;
    float evidence = 0.0f;
    float confidence = 0.0f;
    uint32_t supporting_episode_count = 0;
};

struct BrainStateHeader {
    uint32_t format_version = 1;
    uint32_t state_schema_version = 1;
    uint64_t seed = 0;
    uint64_t tick = 0;
    uint64_t manifest_hash = 0;
    uint32_t endian_marker = 0x01020304;
    uint32_t section_count = 0;
};

struct MemoryConfig {
    float capture_novelty_weight = 1.0f;
    float capture_error_weight = 1.0f;
    float capture_reward_weight = 1.0f;
    float capture_utility_weight = 0.5f;
    float capture_uncertainty_weight = 0.5f;
    float minimum_capture_score = 0.0f;
    uint32_t max_episode_events = 4096;
    uint32_t max_replay_events = 10000;
    uint64_t max_replay_ticks = 100000;
    uint32_t replay_quota_per_context = 4;
    uint32_t max_working_slots = 64;
};

class StableIdArena {
public:
    StableIdArena() = default;

    void initialize(size_t neuron_capacity, size_t synapse_capacity) {
        neuron_capacity_ = neuron_capacity;
        synapse_capacity_ = synapse_capacity;
        neuron_ids_.clear();
        synapse_ids_.clear();
        next_neuron_id_ = 1;
        next_synapse_id_ = 1;
    }

    uint64_t allocate_neuron() {
        if (neuron_ids_.size() >= neuron_capacity_) throw MemoryError("neuron arena capacity exhausted");
        neuron_ids_.push_back(next_neuron_id_++);
        return neuron_ids_.back();
    }

    uint64_t allocate_synapse() {
        if (synapse_ids_.size() >= synapse_capacity_) throw MemoryError("synapse arena capacity exhausted");
        synapse_ids_.push_back(next_synapse_id_++);
        return synapse_ids_.back();
    }

    void reserve_transaction(size_t new_neuron_capacity, size_t new_synapse_capacity, bool force_failure = false) {
        if (new_neuron_capacity < neuron_ids_.size() || new_synapse_capacity < synapse_ids_.size()) {
            throw MemoryError("transaction would discard active stable IDs");
        }
        const auto old_neurons = neuron_ids_;
        const auto old_synapses = synapse_ids_;
        const size_t old_neuron_capacity = neuron_capacity_;
        const size_t old_synapse_capacity = synapse_capacity_;
        try {
            if (force_failure) throw MemoryError("forced migration failure");
            std::vector<uint64_t> migrated_neurons = neuron_ids_;
            std::vector<uint64_t> migrated_synapses = synapse_ids_;
            neuron_capacity_ = new_neuron_capacity;
            synapse_capacity_ = new_synapse_capacity;
            neuron_ids_.swap(migrated_neurons);
            synapse_ids_.swap(migrated_synapses);
        } catch (...) {
            neuron_ids_ = old_neurons;
            synapse_ids_ = old_synapses;
            neuron_capacity_ = old_neuron_capacity;
            synapse_capacity_ = old_synapse_capacity;
            throw;
        }
    }

    size_t neuron_capacity() const { return neuron_capacity_; }
    size_t synapse_capacity() const { return synapse_capacity_; }
    const std::vector<uint64_t>& neuron_ids() const { return neuron_ids_; }
    const std::vector<uint64_t>& synapse_ids() const { return synapse_ids_; }

    uint64_t hash() const {
        uint64_t value = 1469598103934665603ULL;
        auto mix = [&value](uint64_t input) {
            for (int i = 0; i < 8; ++i) {
                value ^= static_cast<uint8_t>(input & 0xffU);
                value *= 1099511628211ULL;
                input >>= 8;
            }
        };
        mix(neuron_capacity_);
        mix(synapse_capacity_);
        for (uint64_t id : neuron_ids_) mix(id);
        for (uint64_t id : synapse_ids_) mix(id);
        return value;
    }

private:
    friend class MemorySystem;
    size_t neuron_capacity_ = 0;
    size_t synapse_capacity_ = 0;
    uint64_t next_neuron_id_ = 1;
    uint64_t next_synapse_id_ = 1;
    std::vector<uint64_t> neuron_ids_;
    std::vector<uint64_t> synapse_ids_;
};

class MemorySystem {
public:
    using ReplayCallback = std::function<void(const MemoryEvent&, ReplayMode)>;

    explicit MemorySystem(uint64_t seed = 424242, const MemoryConfig& config = MemoryConfig{})
        : seed_(seed), config_(config) {}

    void initialize_arena(size_t neuron_capacity, size_t synapse_capacity) {
        arena_.initialize(neuron_capacity, synapse_capacity);
    }

    StableIdArena& arena() { return arena_; }
    const StableIdArena& arena() const { return arena_; }
    const MemoryConfig& config() const { return config_; }
    void set_replay_callback(ReplayCallback callback) { replay_callback_ = std::move(callback); }

    uint64_t begin_episode(const EventHeader& context) {
        if (active_episode_id_ != 0) throw MemoryError("an episode is already active");
        active_episode_id_ = next_episode_id_++;
        active_start_tick_ = context.tick;
        active_context_pointer_id_ = context.payload_id;
        active_context_hash_ = context.provenance_id ^ context.payload_id;
        active_event_offset_ = events_.size();
        active_events_.clear();
        append_event(active_episode_id_, context);
        return active_episode_id_;
    }

    void append_event(uint64_t episode_id, const EventHeader& header) {
        if (episode_id == 0 || episode_id != active_episode_id_) throw MemoryError("append_event references a non-active episode");
        if (active_events_.size() >= config_.max_episode_events) throw MemoryError("episode event budget exceeded");
        if (!active_events_.empty() && header.tick < active_events_.back().header.tick) throw MemoryError("episode event timestamp moved backward");
        MemoryEvent event{};
        event.header = header;
        event.context_hash = active_context_hash_;
        event.value = static_cast<float>(header.event_type);
        active_events_.push_back(event);
    }

    void append_event(uint64_t episode_id, const MemoryEvent& event) {
        if (episode_id == 0 || episode_id != active_episode_id_) throw MemoryError("append_event references a non-active episode");
        if (active_events_.size() >= config_.max_episode_events) throw MemoryError("episode event budget exceeded");
        if (!active_events_.empty() && event.header.tick < active_events_.back().header.tick) throw MemoryError("episode event timestamp moved backward");
        active_events_.push_back(event);
    }

    void set_active_metrics(float novelty, float prediction_error, float reward, float future_utility, float uncertainty_reduction = 0.0f) {
        active_novelty_ = novelty;
        active_prediction_error_ = prediction_error;
        active_reward_ = reward;
        active_future_utility_ = future_utility;
        active_uncertainty_reduction_ = uncertainty_reduction;
    }

    EpisodeRecord close_episode(uint64_t episode_id, float outcome) {
        if (episode_id == 0 || episode_id != active_episode_id_) throw MemoryError("close_episode references a non-active episode");
        if (active_events_.empty()) throw MemoryError("cannot close an empty episode");
        const float capture = capture_score();
        EpisodeRecord record{};
        record.episode_id = episode_id;
        record.start_tick = active_start_tick_;
        record.end_tick = active_events_.back().header.tick;
        record.context_pointer_id = active_context_pointer_id_;
        record.event_offset = events_.size();
        record.event_count = static_cast<uint32_t>(active_events_.size());
        record.novelty = active_novelty_;
        record.prediction_error = active_prediction_error_;
        record.reward = outcome == 0.0f ? active_reward_ : outcome;
        record.future_utility = active_future_utility_;
        record.checksum = checksum_events(active_events_);
        if (capture >= config_.minimum_capture_score) {
            const size_t index = episodes_.size();
            events_.insert(events_.end(), active_events_.begin(), active_events_.end());
            episodes_.push_back(record);
            ReplayIndex replay_index{};
            replay_index.episode_id = record.episode_id;
            replay_index.cue_hash = cue_hash(active_events_);
            replay_index.context_hash = active_context_hash_;
            replay_index.retrieval_strength = 1.0f;
            replay_index.replay_priority = capture + std::abs(record.reward);
            replay_index.access_count = 0;
            replay_index.flags = 0;
            replay_indices_.push_back(replay_index);
            (void)index;
        }
        active_episode_id_ = 0;
        active_events_.clear();
        return record;
    }

    void timeout(uint64_t current_tick) {
        if (active_episode_id_ == 0 || active_events_.empty()) return;
        if (current_tick - active_start_tick_ >= config_.max_episode_events) close_episode(active_episode_id_, active_reward_);
    }

    const std::vector<EpisodeRecord>& episodes() const { return episodes_; }
    const std::vector<MemoryEvent>& events() const { return events_; }
    const std::vector<ReplayIndex>& replay_indices() const { return replay_indices_; }
    const std::vector<ConsolidationCandidate>& candidates() const { return candidates_; }
    float last_capture_score() const { return last_capture_score_; }

    std::vector<ReplayIndex> select_replay(const ReplayQuery& query, size_t budget) {
        std::vector<std::pair<float, size_t>> ranked;
        for (size_t i = 0; i < replay_indices_.size(); ++i) {
            const ReplayIndex& index = replay_indices_[i];
            if (index.replay_priority < query.minimum_priority) continue;
            const EpisodeRecord* episode = find_episode(index.episode_id);
            if (!episode) continue;
            if (query.current_tick >= episode->end_tick && query.current_tick - episode->end_tick > query.max_age_ticks) continue;
            float score = index.replay_priority / (1.0f + static_cast<float>(index.access_count));
            if (query.cue_hash != 0 && query.cue_hash == index.cue_hash) score += 1000.0f;
            if (query.context_hash != 0 && query.context_hash == index.context_hash) score += 100.0f;
            ranked.emplace_back(score, i);
        }
        std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second < b.second;
        });
        std::vector<ReplayIndex> selected;
        std::map<uint64_t, uint32_t> context_counts;
        for (const auto& entry : ranked) {
            if (selected.size() >= budget) break;
            const ReplayIndex& candidate = replay_indices_[entry.second];
            const EpisodeRecord* episode = find_episode(candidate.episode_id);
            const uint64_t context = episode ? episode->context_pointer_id : 0;
            if (context_counts[context] >= config_.replay_quota_per_context && selected.size() + 1 < budget) continue;
            ++context_counts[context];
            ++replay_indices_[entry.second].access_count;
            selected.push_back(replay_indices_[entry.second]);
        }
        return selected;
    }

    ReplayReport replay(const std::vector<ReplayIndex>& selected, ReplayMode mode = ReplayMode::ExactEvent,
                        uint64_t event_budget = std::numeric_limits<uint64_t>::max()) const {
        ReplayReport report;
        report.selected_episodes = static_cast<uint32_t>(selected.size());
        report.budget_ticks = std::min<uint64_t>(config_.max_replay_ticks, event_budget);
        uint64_t used = 0;
        double error_sum = 0.0;
        for (const ReplayIndex& index : selected) {
            const EpisodeRecord* episode = find_episode(index.episode_id);
            if (!episode) continue;
            const auto episode_events = events_for(*episode);
            if (mode == ReplayMode::CompressedPointer) {
                ++report.pointer_replays;
                if (replay_callback_ && !episode_events.empty()) replay_callback_(episode_events.front(), mode);
                ++used;
                continue;
            }
            if (mode == ReplayMode::GenerativeReconstruction) {
                ++report.generated_replays;
                for (const MemoryEvent& event : episode_events) {
                    if (used >= event_budget || used >= config_.max_replay_events) break;
                    MemoryEvent reconstructed = event;
                    reconstructed.header.provenance_id ^= 0x9e3779b97f4a7c15ULL;
                    if (replay_callback_) replay_callback_(reconstructed, mode);
                    ++report.replayed_events;
                    ++used;
                }
                error_sum += episode_events.empty() ? 1.0 : 0.0;
                continue;
            }
            for (const MemoryEvent& event : episode_events) {
                if (used >= event_budget || used >= config_.max_replay_events) break;
                if (replay_callback_) replay_callback_(event, mode);
                ++report.replayed_events;
                ++used;
            }
        }
        const uint32_t denominator = report.generated_replays == 0 ? 0 : report.generated_replays;
        report.mean_reconstruction_error = denominator == 0 ? 0.0f : static_cast<float>(error_sum / denominator);
        report.replayed_events += report.pointer_replays;
        return report;
    }

    std::vector<ConsolidationCandidate> emit_candidates() {
        struct Aggregate { float evidence = 0.0f; uint32_t count = 0; };
        std::map<std::pair<uint64_t, uint64_t>, Aggregate> aggregate;
        for (const EpisodeRecord& episode : episodes_) {
            auto& item = aggregate[{episode.context_pointer_id, episode.episode_id}];
            item.evidence += std::abs(episode.reward) + episode.novelty + episode.prediction_error;
            ++item.count;
        }
        candidates_.clear();
        for (const auto& item : aggregate) {
            ConsolidationCandidate candidate;
            candidate.pointer_id = item.first.first;
            candidate.context_id = item.first.second;
            candidate.relation_type = 1;
            candidate.evidence = item.second.evidence;
            candidate.supporting_episode_count = item.second.count;
            candidate.confidence = std::min(1.0f, candidate.evidence / std::max(1.0f, candidate.supporting_episode_count * 3.0f));
            candidates_.push_back(candidate);
        }
        return candidates_;
    }

    void resize_transaction(size_t neuron_capacity, size_t synapse_capacity, bool force_failure = false) {
        arena_.reserve_transaction(neuron_capacity, synapse_capacity, force_failure);
    }

    uint64_t state_hash() const {
        uint64_t hash = 1469598103934665603ULL;
        auto mix = [&hash](const void* data, size_t bytes) {
            const auto* p = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < bytes; ++i) { hash ^= p[i]; hash *= 1099511628211ULL; }
        };
        const auto mix_value = [&mix](auto value) { mix(&value, sizeof(value)); };
        mix_value(seed_); mix_value(tick_); mix_value(next_episode_id_); mix_value(arena_.hash());
        for (const auto& episode : episodes_) mix(&episode, sizeof(episode));
        for (const auto& event : events_) mix(&event, sizeof(event));
        for (const auto& index : replay_indices_) mix(&index, sizeof(index));
        return hash;
    }

    void set_tick(uint64_t tick) { tick_ = tick; }
    uint64_t tick() const { return tick_; }
    uint64_t seed() const { return seed_; }

    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);

private:
    struct ActiveState {
        uint64_t episode_id = 0;
        uint64_t start_tick = 0;
        uint64_t context_pointer_id = 0;
        uint64_t context_hash = 0;
        uint64_t event_offset = 0;
        std::vector<MemoryEvent> events;
        float novelty = 0.0f;
        float prediction_error = 0.0f;
        float reward = 0.0f;
        float future_utility = 0.0f;
        float uncertainty_reduction = 0.0f;
    };

    float capture_score() {
        last_capture_score_ = config_.capture_novelty_weight * active_novelty_ +
                              config_.capture_error_weight * active_prediction_error_ +
                              config_.capture_reward_weight * std::abs(active_reward_) +
                              config_.capture_utility_weight * active_future_utility_ +
                              config_.capture_uncertainty_weight * active_uncertainty_reduction_;
        return last_capture_score_;
    }

    static uint32_t checksum_bytes(const uint8_t* data, size_t size) {
        uint32_t hash = 2166136261U;
        for (size_t i = 0; i < size; ++i) { hash ^= data[i]; hash *= 16777619U; }
        return hash;
    }

    static uint32_t checksum_events(const std::vector<MemoryEvent>& events) {
        return checksum_bytes(reinterpret_cast<const uint8_t*>(events.data()), events.size() * sizeof(MemoryEvent));
    }

    static uint64_t cue_hash(const std::vector<MemoryEvent>& events) {
        uint64_t hash = 1469598103934665603ULL;
        for (const auto& event : events) {
            hash ^= event.header.payload_id; hash *= 1099511628211ULL;
            hash ^= event.header.event_type; hash *= 1099511628211ULL;
        }
        return hash;
    }

    const EpisodeRecord* find_episode(uint64_t episode_id) const {
        for (const auto& episode : episodes_) if (episode.episode_id == episode_id) return &episode;
        return nullptr;
    }

    std::vector<MemoryEvent> events_for(const EpisodeRecord& episode) const {
        if (episode.event_offset + episode.event_count > events_.size()) throw MemoryError("episode event range is invalid");
        return std::vector<MemoryEvent>(events_.begin() + static_cast<std::ptrdiff_t>(episode.event_offset),
                                        events_.begin() + static_cast<std::ptrdiff_t>(episode.event_offset + episode.event_count));
    }

    uint64_t seed_ = 424242;
    uint64_t tick_ = 0;
    uint64_t next_episode_id_ = 1;
    MemoryConfig config_;
    StableIdArena arena_;
    std::vector<MemoryEvent> events_;
    std::vector<EpisodeRecord> episodes_;
    std::vector<ReplayIndex> replay_indices_;
    std::vector<ConsolidationCandidate> candidates_;
    ReplayCallback replay_callback_;

    uint64_t active_episode_id_ = 0;
    uint64_t active_start_tick_ = 0;
    uint64_t active_context_pointer_id_ = 0;
    uint64_t active_context_hash_ = 0;
    uint64_t active_event_offset_ = 0;
    std::vector<MemoryEvent> active_events_;
    float active_novelty_ = 0.0f;
    float active_prediction_error_ = 0.0f;
    float active_reward_ = 0.0f;
    float active_future_utility_ = 0.0f;
    float active_uncertainty_reduction_ = 0.0f;
    float last_capture_score_ = 0.0f;
};

} // namespace genesis
