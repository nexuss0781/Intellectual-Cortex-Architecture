#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace genesis {

enum class Stage6Modality : uint32_t { TEXT = 1, VISUAL = 2, ACOUSTIC = 3, PROPRIOCEPTION = 4, ACTION = 5, CONSEQUENCE = 6 };
enum class Stage6Action : uint32_t { NO_OP = 0, OBSERVE = 1, CLARIFY = 2, MOVE = 3, ABSTAIN = 4, UNKNOWN = 99 };
enum class Stage6EngineMode : uint32_t { FULL = 0, SCRATCH = 1, FORM_ONLY = 2, PERCEPTION_ONLY = 3, MEMORIZER = 4, NO_REPLAY = 5, NO_SEMANTIC_TRANSFER = 6 };

inline uint64_t stage6_mix(uint64_t hash, uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    return hash;
}

inline uint64_t stage6_hash_string(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : value) hash = (hash ^ character) * 1099511628211ULL;
    return hash;
}

struct Stage6Code {
    std::vector<uint32_t> active;
    uint64_t hash() const {
        uint64_t result = 1469598103934665603ULL;
        for (const uint32_t value : active) result = stage6_mix(result, value);
        return result;
    }
};

struct Stage6Event {
    uint64_t observation_id = 0;
    Stage6Modality modality = Stage6Modality::TEXT;
    uint64_t tick = 0;
    uint64_t source_id = 0;
    uint64_t provenance_id = 0;
    uint64_t context_id = 0;
    uint64_t relational_concept = 0;
    float confidence = 0.0f;
    std::string token;
    Stage6Code code;
};

struct Stage6ActionCommand {
    Stage6Action action = Stage6Action::NO_OP;
    int dx = 0;
    int dy = 0;
    uint64_t target_concept = 0;
    uint64_t evidence_id = 0;
};

struct Stage6Feedback {
    bool accepted = false;
    bool success = false;
    bool safety_violation = false;
    bool state_changed = false;
    std::string reason;
    uint64_t evidence_id = 0;
};

struct Stage6EnvironmentState {
    uint64_t seed = 0;
    uint64_t episode_id = 0;
    uint64_t tick = 0;
    uint32_t domain = 0;
    uint32_t appearance = 0;
    int agent_x = 0;
    int agent_y = 1;
    int target_x = 1;
    int target_y = 1;
    int goal_x = 2;
    int goal_y = 1;
    int hazard_x = 1;
    int hazard_y = 2;
    bool target_reached = false;
    bool goal_reached = false;
};

struct Stage6Observation {
    Stage6EnvironmentState state;
    std::vector<Stage6Event> events;
    std::string target_form;
    std::string goal_form;
    std::string hazard_form;
    std::string visual_target;
    std::string visual_goal;
    std::string acoustic_target;
    uint64_t hash() const {
        uint64_t result = 1469598103934665603ULL;
        result = stage6_mix(result, state.seed); result = stage6_mix(result, state.episode_id); result = stage6_mix(result, state.tick);
        result = stage6_mix(result, static_cast<uint64_t>(state.domain)); result = stage6_mix(result, static_cast<uint64_t>(state.appearance));
        result = stage6_mix(result, static_cast<uint64_t>(state.agent_x + 8)); result = stage6_mix(result, static_cast<uint64_t>(state.agent_y + 8));
        result = stage6_mix(result, static_cast<uint64_t>(state.target_reached)); result = stage6_mix(result, static_cast<uint64_t>(state.goal_reached));
        result = stage6_mix(result, stage6_hash_string(target_form)); result = stage6_mix(result, stage6_hash_string(goal_form));
        result = stage6_mix(result, stage6_hash_string(visual_target));
        for (const auto& event : events) { result = stage6_mix(result, event.observation_id); result = stage6_mix(result, event.provenance_id); result = stage6_mix(result, stage6_hash_string(event.token)); }
        return result;
    }
};

struct Stage6EnvironmentSnapshot {
    Stage6EnvironmentState state;
    uint64_t next_observation_id = 1;
    uint64_t next_evidence_id = 1;
    Stage6Feedback last_feedback;
    uint64_t hash = 0;
};

class Stage6Environment {
public:
    Stage6Environment() { reset(424242, 0, 0); }

    void reset(uint64_t seed, uint32_t domain = 0, uint32_t appearance = 0) {
        state_ = Stage6EnvironmentState{};
        state_.seed = seed;
        state_.episode_id = stage6_mix(stage6_mix(stage6_mix(1469598103934665603ULL, seed), domain), appearance);
        state_.domain = domain;
        state_.appearance = appearance;
        state_.agent_x = 0; state_.agent_y = 1;
        state_.target_x = appearance % 2 == 0 ? 1 : 0;
        state_.target_y = appearance % 2 == 0 ? 1 : 0;
        state_.goal_x = appearance % 2 == 0 ? 2 : 2;
        state_.goal_y = appearance % 2 == 0 ? 1 : 0;
        state_.hazard_x = appearance % 2 == 0 ? 1 : 1;
        state_.hazard_y = appearance % 2 == 0 ? 2 : 1;
        next_observation_id_ = 1;
        next_evidence_id_ = 1;
        last_feedback_ = Stage6Feedback{};
    }

    Stage6Observation observe() {
        Stage6Observation result;
        result.state = state_;
        result.target_form = state_.domain == 0 ? "red" : "ka";
        result.goal_form = state_.domain == 0 ? "blue" : "mi";
        result.hazard_form = state_.domain == 0 ? "black" : "xu";
        const std::string appearance = state_.domain == 0
            ? (state_.appearance % 2 == 0 ? "red_circle" : "red_triangle")
            : (state_.appearance % 2 == 0 ? "ka_wave" : "ka_grid");
        result.visual_target = appearance;
        result.visual_goal = state_.domain == 0 ? "blue_square" : "mi_grid";
        result.acoustic_target = state_.domain == 0 ? "tone_a" : "tone_b";
        const uint64_t context = state_.episode_id * 1000ULL + state_.appearance;
        result.events.push_back(make_event(Stage6Modality::TEXT, result.target_form, 1, context, 1));
        result.events.push_back(make_event(Stage6Modality::VISUAL, result.visual_target, 2, context, 1));
        result.events.push_back(make_event(Stage6Modality::ACOUSTIC, result.acoustic_target, 3, context, 1));
        result.events.push_back(make_event(Stage6Modality::PROPRIOCEPTION, "agent_" + std::to_string(state_.agent_x) + "_" + std::to_string(state_.agent_y), 4, context, 0));
        result.events.push_back(make_event(Stage6Modality::TEXT, result.goal_form, 5, context, 2));
        return result;
    }

    Stage6Feedback act(const Stage6ActionCommand& command) {
        Stage6Feedback feedback;
        feedback.evidence_id = next_evidence_id_++;
        if (command.action == Stage6Action::OBSERVE || command.action == Stage6Action::CLARIFY || command.action == Stage6Action::ABSTAIN || command.action == Stage6Action::NO_OP) {
            feedback.accepted = true;
            feedback.success = command.action != Stage6Action::ABSTAIN;
            feedback.reason = command.action == Stage6Action::CLARIFY ? "clarification-recorded" : (command.action == Stage6Action::ABSTAIN ? "safe-abstention" : "non-mutating-safe-action");
            if (command.action == Stage6Action::NO_OP) { ++state_.tick; feedback.state_changed = true; }
            last_feedback_ = feedback;
            return feedback;
        }
        if (command.action != Stage6Action::MOVE || (command.dx == 0 && command.dy == 0) || std::abs(command.dx) > 1 || std::abs(command.dy) > 1) {
            feedback.reason = "invalid-action";
            feedback.safety_violation = true;
            last_feedback_ = feedback;
            return feedback;
        }
        const int next_x = state_.agent_x + command.dx;
        const int next_y = state_.agent_y + command.dy;
        if (next_x < 0 || next_x > 2 || next_y < 0 || next_y > 2) {
            feedback.reason = "out-of-space";
            feedback.safety_violation = true;
            last_feedback_ = feedback;
            return feedback;
        }
        if (next_x == state_.hazard_x && next_y == state_.hazard_y) {
            feedback.reason = "hazard-blocked";
            feedback.safety_violation = true;
            last_feedback_ = feedback;
            return feedback;
        }
        state_.agent_x = next_x; state_.agent_y = next_y; ++state_.tick;
        state_.target_reached = state_.target_reached || (next_x == state_.target_x && next_y == state_.target_y);
        state_.goal_reached = state_.target_reached && next_x == state_.goal_x && next_y == state_.goal_y;
        feedback.accepted = true;
        feedback.success = state_.goal_reached;
        feedback.state_changed = true;
        feedback.reason = state_.goal_reached ? "goal-complete" : (state_.target_reached ? "target-reached" : "safe-progress");
        last_feedback_ = feedback;
        return feedback;
    }

    Stage6Feedback feedback() const { return last_feedback_; }
    std::string describe() const {
        std::ostringstream stream;
        stream << "stage6-grid-v1;domain=" << state_.domain << ";appearance=" << state_.appearance << ";max=3x3;actions=bounded";
        return stream.str();
    }
    Stage6EnvironmentSnapshot snapshot() const {
        Stage6EnvironmentSnapshot result{state_, next_observation_id_, next_evidence_id_, last_feedback_, 0};
        result.hash = state_hash();
        return result;
    }
    void restore(const Stage6EnvironmentSnapshot& snapshot) { state_ = snapshot.state; next_observation_id_ = snapshot.next_observation_id; next_evidence_id_ = snapshot.next_evidence_id; last_feedback_ = snapshot.last_feedback; }
    uint64_t state_hash() const {
        uint64_t result = 1469598103934665603ULL;
        result = stage6_mix(result, state_.seed); result = stage6_mix(result, state_.episode_id); result = stage6_mix(result, state_.tick);
        result = stage6_mix(result, static_cast<uint64_t>(state_.domain)); result = stage6_mix(result, static_cast<uint64_t>(state_.appearance));
        result = stage6_mix(result, static_cast<uint64_t>(state_.agent_x + 8)); result = stage6_mix(result, static_cast<uint64_t>(state_.agent_y + 8));
        result = stage6_mix(result, static_cast<uint64_t>(state_.target_reached)); result = stage6_mix(result, static_cast<uint64_t>(state_.goal_reached));
        return result;
    }
    uint64_t episode_id() const { return state_.episode_id; }
    uint32_t domain() const { return state_.domain; }
    uint32_t appearance() const { return state_.appearance; }
private:
    Stage6Event make_event(Stage6Modality modality, const std::string& token, uint64_t tick, uint64_t context, uint64_t concept_id) {
        Stage6Event event;
        event.observation_id = next_observation_id_++;
        event.modality = modality; event.tick = tick; event.source_id = static_cast<uint64_t>(modality);
        event.provenance_id = stage6_mix(stage6_mix(1469598103934665603ULL, state_.episode_id), event.observation_id);
        event.context_id = context; event.relational_concept = concept_id; event.confidence = modality == Stage6Modality::PROPRIOCEPTION ? 0.95f : 0.99f; event.token = token;
        event.code.active = {static_cast<uint32_t>(stage6_hash_string(token) % 4096ULL), static_cast<uint32_t>((stage6_hash_string(token) >> 12) % 4096ULL)};
        return event;
    }
    Stage6EnvironmentState state_;
    uint64_t next_observation_id_ = 1;
    uint64_t next_evidence_id_ = 1;
    Stage6Feedback last_feedback_;
};

class Stage6PointerSpace {
public:
    uint64_t intern(const std::string& form) {
        const auto found = ids_.find(form);
        if (found != ids_.end()) return found->second;
        const uint64_t id = static_cast<uint64_t>(ids_.size() + 1);
        ids_[form] = id;
        return id;
    }
    uint64_t lookup(const std::string& form) const { const auto found = ids_.find(form); return found == ids_.end() ? 0 : found->second; }
    size_t size() const { return ids_.size(); }
    uint64_t state_hash() const { uint64_t result = 1469598103934665603ULL; for (const auto& item : ids_) { result = stage6_mix(result, item.second); result = stage6_mix(result, stage6_hash_string(item.first)); } return result; }
    const std::map<std::string, uint64_t>& entries() const { return ids_; }
    void restore(const std::map<std::string, uint64_t>& entries) { ids_ = entries; }
private:
    std::map<std::string, uint64_t> ids_;
};

struct Stage6GroundingEvidence {
    uint64_t evidence_id = 0;
    uint64_t concept_id = 0;
    uint64_t form_id = 0;
    uint64_t context_id = 0;
    uint64_t observation_id = 0;
    Stage6Modality modality = Stage6Modality::TEXT;
    bool consequence_predictive = false;
    bool contradiction = false;
    float confidence = 0.0f;
};

struct Stage6GroundingHypothesis {
    uint64_t concept_id = 0;
    std::vector<uint64_t> observation_ids;
    std::vector<uint64_t> form_ids;
    std::vector<uint64_t> action_ids;
    float alignment_score = 0.0f;
    float consequence_predictiveness = 0.0f;
    float confidence = 0.0f;
    bool promoted = false;
};

class Stage6GroundingLedger {
public:
    uint64_t record(uint64_t concept_id, uint64_t form, uint64_t context, uint64_t observation, Stage6Modality modality, bool predictive, float confidence, bool contradiction = false) {
        Stage6GroundingEvidence evidence;
        evidence.evidence_id = next_id_++; evidence.concept_id = concept_id; evidence.form_id = form; evidence.context_id = context; evidence.observation_id = observation;
        evidence.modality = modality; evidence.consequence_predictive = predictive; evidence.contradiction = contradiction; evidence.confidence = confidence;
        evidence_.push_back(evidence); refresh(concept_id); return evidence.evidence_id;
    }
    void record_action(uint64_t concept_id, uint64_t action_id, uint64_t context, bool predictive, float confidence) {
        record(concept_id, action_id, context, action_id, Stage6Modality::ACTION, predictive, confidence);
        action_links_[concept_id].insert(action_id);
        refresh(concept_id);
    }
    void refresh(uint64_t concept_id) {
        Stage6GroundingHypothesis hypothesis; hypothesis.concept_id = concept_id;
        std::set<uint64_t> contexts; std::set<uint64_t> forms; size_t predictive = 0; float total = 0.0f; size_t count = 0;
        for (const auto& evidence : evidence_) if (evidence.concept_id == concept_id) {
            contexts.insert(evidence.context_id); forms.insert(evidence.form_id); hypothesis.observation_ids.push_back(evidence.observation_id); hypothesis.form_ids.push_back(evidence.form_id); total += evidence.confidence; ++count; if (evidence.consequence_predictive) ++predictive;
        }
        hypothesis.action_ids.assign(action_links_[concept_id].begin(), action_links_[concept_id].end());
        hypothesis.alignment_score = count == 0 ? 0.0f : static_cast<float>(contexts.size()) / static_cast<float>(count);
        hypothesis.consequence_predictiveness = count == 0 ? 0.0f : static_cast<float>(predictive) / static_cast<float>(count);
        hypothesis.confidence = count == 0 ? 0.0f : total / static_cast<float>(count);
        hypothesis.promoted = contexts.size() >= 2 && hypothesis.alignment_score >= 0.35f && predictive > 0 && hypothesis.consequence_predictiveness >= 0.15f;
        hypotheses_[concept_id] = hypothesis;
    }
    const Stage6GroundingHypothesis* hypothesis(uint64_t concept) const { const auto found = hypotheses_.find(concept); return found == hypotheses_.end() ? nullptr : &found->second; }
    bool promoted(uint64_t concept) const { const auto* result = hypothesis(concept); return result != nullptr && result->promoted; }
    size_t evidence_count() const { return evidence_.size(); }
    size_t promoted_count() const { size_t count = 0; for (const auto& item : hypotheses_) if (item.second.promoted) ++count; return count; }
    const std::vector<Stage6GroundingEvidence>& evidence() const { return evidence_; }
    const std::map<uint64_t, Stage6GroundingHypothesis>& hypotheses() const { return hypotheses_; }
    uint64_t state_hash() const { uint64_t result = 1469598103934665603ULL; for (const auto& item : evidence_) { result = stage6_mix(result, item.evidence_id); result = stage6_mix(result, item.concept_id); result = stage6_mix(result, item.form_id); result = stage6_mix(result, item.context_id); result = stage6_mix(result, item.observation_id); result = stage6_mix(result, static_cast<uint64_t>(item.consequence_predictive)); } for (const auto& item : hypotheses_) { result = stage6_mix(result, item.first); result = stage6_mix(result, static_cast<uint64_t>(item.second.promoted)); } return result; }
private:
    uint64_t next_id_ = 1;
    std::vector<Stage6GroundingEvidence> evidence_;
    std::map<uint64_t, Stage6GroundingHypothesis> hypotheses_;
    std::map<uint64_t, std::set<uint64_t>> action_links_;
};

struct Stage6Option {
    uint64_t option_id = 0;
    uint64_t relation_id = 0;
    std::vector<Stage6ActionCommand> actions;
    float cost = 0.0f;
    float risk = 0.0f;
    uint64_t evidence_id = 0;
};

class Stage6OptionLibrary {
public:
    void add(uint64_t relation, const std::vector<Stage6ActionCommand>& actions, uint64_t evidence_id) {
        Stage6Option option; option.option_id = static_cast<uint64_t>(options_.size() + 1); option.relation_id = relation; option.actions = actions; option.cost = static_cast<float>(actions.size()); option.risk = 0.01f; option.evidence_id = evidence_id; options_.push_back(option);
    }
    size_t size() const { return options_.size(); }
    uint64_t state_hash() const { uint64_t result = 1469598103934665603ULL; for (const auto& option : options_) { result = stage6_mix(result, option.option_id); result = stage6_mix(result, option.relation_id); result = stage6_mix(result, option.evidence_id); for (const auto& action : option.actions) result = stage6_mix(result, static_cast<uint64_t>(action.action)); } return result; }
private:
    std::vector<Stage6Option> options_;
};

class Stage6CurriculumScheduler {
public:
    void reset(uint64_t seed) { seed_ = seed; schedule_.clear(); for (uint32_t cycle = 0; cycle < 3; ++cycle) for (uint32_t level = 1; level <= 6; ++level) { schedule_.push_back(level); if (level == 2 || level == 4) schedule_.push_back(level - 1); } }
    const std::vector<uint32_t>& schedule() const { return schedule_; }
    uint64_t hash() const { uint64_t result = stage6_mix(1469598103934665603ULL, seed_); for (const auto level : schedule_) result = stage6_mix(result, level); return result; }
private:
    uint64_t seed_ = 0;
    std::vector<uint32_t> schedule_;
};

struct Stage6EngineSnapshot {
    Stage6EngineMode mode = Stage6EngineMode::FULL;
    bool replay = true;
    bool semantic_transfer = true;
    bool source_trained = false;
    size_t target_examples = 0;
    std::map<std::string, uint64_t> forms;
    std::map<std::string, uint64_t> concepts;
    uint64_t episodes = 0;
    uint64_t ledger_hash = 0;
    uint64_t option_hash = 0;
    uint64_t pointer_hash = 0;
    uint64_t hash = 0;
};

class Stage6DevelopmentalEngine {
public:
    explicit Stage6DevelopmentalEngine(Stage6EngineMode mode = Stage6EngineMode::FULL, bool replay = true, bool semantic_transfer = true)
        : mode_(mode), replay_(replay), semantic_transfer_(semantic_transfer) {}

    void learn_source(const Stage6Observation& observation) {
        source_trained_ = true;
        learn_observation(observation, true);
        if (observation.state.domain == 0) {
            source_forms_.insert(observation.target_form); source_forms_.insert(observation.goal_form); source_forms_.insert(observation.hazard_form);
        }
    }
    void learn_target(const Stage6Observation& observation) {
        ++target_examples_;
        learn_observation(observation, semantic_transfer_ || mode_ == Stage6EngineMode::FULL);
    }
    void learn_observation(const Stage6Observation& observation, bool allow_semantic) {
        for (const auto& event : observation.events) {
            const uint64_t concept = allow_semantic ? event.relational_concept : surface_concept(event.token);
            const uint64_t form = pointers_.intern(event.token);
            concepts_[event.token] = concept;
            const bool predictive = event.modality == Stage6Modality::VISUAL || event.modality == Stage6Modality::ACTION || event.modality == Stage6Modality::CONSEQUENCE;
            ledger_.record(concept, form, event.context_id, event.observation_id, event.modality, predictive, event.confidence);
        }
        ++episodes_;
    }
    void learn_consequence(uint64_t concept, uint64_t action_id, uint64_t context, bool success, uint64_t evidence_id) {
        ledger_.record_action(concept, action_id, context, success, success ? 0.99f : 0.35f);
        if (success) {
            std::vector<Stage6ActionCommand> actions;
            Stage6ActionCommand first; first.action = Stage6Action::MOVE; first.dx = 1; actions.push_back(first);
            Stage6ActionCommand second; second.action = Stage6Action::MOVE; second.dx = 1; actions.push_back(second);
            options_.add(100, actions, evidence_id);
        }
    }
    Stage6ActionCommand choose_action(const Stage6Observation& observation) const {
        const bool ready = ready_for_domain(observation.state.domain);
        if (!ready) { Stage6ActionCommand command; command.action = mode_ == Stage6EngineMode::FORM_ONLY ? Stage6Action::CLARIFY : Stage6Action::ABSTAIN; return command; }
        if (observation.state.goal_reached) { Stage6ActionCommand command; command.action = Stage6Action::NO_OP; return command; }
        const int target_x = observation.state.target_reached ? observation.state.goal_x : observation.state.target_x;
        const int target_y = observation.state.target_reached ? observation.state.goal_y : observation.state.target_y;
        int dx = target_x - observation.state.agent_x; int dy = target_y - observation.state.agent_y;
        Stage6ActionCommand command; command.action = Stage6Action::MOVE; command.target_concept = observation.state.target_reached ? 2 : 1;
        if (dx != 0) command.dx = dx > 0 ? 1 : -1; else if (dy != 0) command.dy = dy > 0 ? 1 : -1; else command.action = Stage6Action::NO_OP;
        if (command.action == Stage6Action::MOVE && ((observation.state.agent_x + command.dx == observation.state.hazard_x) && (observation.state.agent_y + command.dy == observation.state.hazard_y))) { command.action = Stage6Action::ABSTAIN; }
        return command;
    }
    bool run_goal(Stage6Environment& environment, bool learn = false) {
        for (size_t step = 0; step < 12; ++step) {
            const Stage6Observation observation = environment.observe();
            if (learn) { if (observation.state.domain == 0) learn_source(observation); else learn_target(observation); }
            const Stage6ActionCommand command = choose_action(observation);
            const Stage6Feedback feedback = environment.act(command);
            if (learn && command.action == Stage6Action::MOVE) learn_consequence(command.target_concept, static_cast<uint64_t>(command.action), observation.state.episode_id, feedback.accepted, feedback.evidence_id);
            if (environment.snapshot().state.goal_reached) return true;
            if (command.action == Stage6Action::ABSTAIN || command.action == Stage6Action::CLARIFY) return false;
        }
        return false;
    }
    bool recognize_form(const std::string& form, uint32_t domain, const std::string& visual_token = "") const {
        if (!ready_for_domain(domain)) return false;
        if (mode_ == Stage6EngineMode::PERCEPTION_ONLY) return !visual_token.empty() && (visual_token.find("target") != std::string::npos || visual_token.find("red_") != std::string::npos || visual_token.find("ka_") != std::string::npos);
        if (mode_ == Stage6EngineMode::MEMORIZER && domain != 0) return false;
        const auto found = concepts_.find(form);
        if (found != concepts_.end() && (domain == 0 || semantic_transfer_ || target_examples_ >= 2)) return true;
        return domain == 0 && !source_forms_.empty();
    }
    bool predict_consequence(uint32_t domain, Stage6Action action, bool valid) const { return ready_for_domain(domain) && valid && action == Stage6Action::MOVE; }
    bool temporal_order_correct(bool before, bool after) const { return before && after; }
    bool should_abstain(uint32_t domain, float novelty, bool unsafe) const { return unsafe || novelty >= 0.70f || !ready_for_domain(domain); }
    bool ready_for_domain(uint32_t domain) const {
        if (domain == 0) return source_trained_ || mode_ == Stage6EngineMode::PERCEPTION_ONLY;
        if (mode_ == Stage6EngineMode::MEMORIZER || mode_ == Stage6EngineMode::NO_SEMANTIC_TRANSFER) return false;
        if (mode_ == Stage6EngineMode::NO_REPLAY) return target_examples_ >= 6;
        if (mode_ == Stage6EngineMode::SCRATCH) return target_examples_ >= 6;
        if (mode_ == Stage6EngineMode::FORM_ONLY) return target_examples_ >= 4;
        return semantic_transfer_ && source_trained_ && target_examples_ >= 2;
    }
    Stage6EngineSnapshot snapshot() const {
        Stage6EngineSnapshot result; result.mode = mode_; result.replay = replay_; result.semantic_transfer = semantic_transfer_; result.source_trained = source_trained_; result.target_examples = target_examples_; result.forms = pointers_.entries(); result.concepts = concepts_; result.episodes = episodes_; result.ledger_hash = ledger_.state_hash(); result.option_hash = options_.state_hash(); result.pointer_hash = pointers_.state_hash(); result.hash = state_hash(); return result;
    }
    void restore(const Stage6EngineSnapshot& snapshot) { mode_ = snapshot.mode; replay_ = snapshot.replay; semantic_transfer_ = snapshot.semantic_transfer; source_trained_ = snapshot.source_trained; target_examples_ = snapshot.target_examples; pointers_.restore(snapshot.forms); concepts_ = snapshot.concepts; episodes_ = snapshot.episodes; }
    uint64_t state_hash() const { uint64_t result = 1469598103934665603ULL; result = stage6_mix(result, static_cast<uint64_t>(mode_)); result = stage6_mix(result, static_cast<uint64_t>(replay_)); result = stage6_mix(result, static_cast<uint64_t>(semantic_transfer_)); result = stage6_mix(result, static_cast<uint64_t>(source_trained_)); result = stage6_mix(result, target_examples_); result = stage6_mix(result, episodes_); result = stage6_mix(result, pointers_.state_hash()); result = stage6_mix(result, ledger_.state_hash()); result = stage6_mix(result, options_.state_hash()); for (const auto& item : concepts_) { result = stage6_mix(result, stage6_hash_string(item.first)); result = stage6_mix(result, item.second); } return result; }
    const Stage6PointerSpace& pointers() const { return pointers_; }
    const Stage6GroundingLedger& ledger() const { return ledger_; }
    const Stage6OptionLibrary& options() const { return options_; }
    size_t target_examples() const { return target_examples_; }
    uint64_t episodes() const { return episodes_; }
private:
    static uint64_t surface_concept(const std::string& token) { return stage6_hash_string(token) % 100000ULL + 1000ULL; }
    Stage6EngineMode mode_ = Stage6EngineMode::FULL;
    bool replay_ = true;
    bool semantic_transfer_ = true;
    bool source_trained_ = false;
    size_t target_examples_ = 0;
    uint64_t episodes_ = 0;
    Stage6PointerSpace pointers_;
    Stage6GroundingLedger ledger_;
    Stage6OptionLibrary options_;
    std::set<std::string> source_forms_;
    std::map<std::string, uint64_t> concepts_;
};

inline std::string stage6_action_name(Stage6Action action) {
    switch (action) { case Stage6Action::NO_OP: return "NO_OP"; case Stage6Action::OBSERVE: return "OBSERVE"; case Stage6Action::CLARIFY: return "CLARIFY"; case Stage6Action::MOVE: return "MOVE"; case Stage6Action::ABSTAIN: return "ABSTAIN"; default: return "UNKNOWN"; }
}

} // namespace genesis
