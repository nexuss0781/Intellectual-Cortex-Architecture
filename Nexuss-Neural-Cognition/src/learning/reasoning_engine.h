#pragma once

#include "language_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace genesis {

enum class ReasoningStatus : uint32_t { VALID = 0, UNKNOWN = 1, INVALID = 2, OUT_OF_SCOPE = 3, ABSTAIN = 4 };

enum class ExecutiveMode : uint32_t { ANSWER = 0, ASK = 1, RETRIEVE = 2, REPLAY = 3, SIMULATE = 4, ACT = 5, ABSTAIN = 6 };

struct EvidenceRef {
    uint64_t episode_id = 0;
    uint64_t event_id = 0;
    uint64_t source_id = 0;
    float reliability = 0.0f;
};

struct BeliefNode {
    uint64_t node_id = 0;
    uint32_t type_id = 0;
    SparseCode pointer;
    float confidence = 0.0f;
    uint64_t revision = 0;
    bool provisional = true;
    std::vector<EvidenceRef> evidence;
};

struct BeliefEdge {
    uint64_t edge_id = 0;
    uint64_t subject_id = 0;
    uint64_t predicate_id = 0;
    uint64_t object_id = 0;
    float confidence = 0.0f;
    std::vector<EvidenceRef> evidence;
    uint64_t revision = 0;
    std::vector<uint64_t> contradiction_links;
    uint64_t valid_from = 0;
    uint64_t valid_until = std::numeric_limits<uint64_t>::max();
};

class BeliefGraph {
public:
    uint64_t add_node(uint64_t node_id, uint32_t type_id, float confidence, const std::vector<EvidenceRef>& evidence = {}) {
        if (node_id == 0 || !std::isfinite(confidence) || confidence < 0.0f || confidence > 1.0f) throw std::invalid_argument("belief node is invalid");
        BeliefNode node; node.node_id = node_id; node.type_id = type_id; node.confidence = confidence; node.revision = ++revision_; node.provisional = evidence.empty(); node.evidence = evidence; nodes_[node_id] = node; return node_id;
    }

    uint64_t add_edge(uint64_t subject_id, uint64_t predicate_id, uint64_t object_id, float confidence, const std::vector<EvidenceRef>& evidence = {}) {
        if (!has_node(subject_id) || !has_node(object_id) || predicate_id == 0 || !std::isfinite(confidence) || confidence < 0.0f || confidence > 1.0f) throw std::invalid_argument("belief edge is invalid");
        BeliefEdge edge; edge.edge_id = next_edge_id_++; edge.subject_id = subject_id; edge.predicate_id = predicate_id; edge.object_id = object_id; edge.confidence = confidence; edge.revision = ++revision_; edge.evidence = evidence; edge.valid_from = revision_;
        for (const auto& item : edges_) if (item.subject_id == subject_id && item.predicate_id == predicate_id && item.object_id != object_id) { edge.contradiction_links.push_back(item.edge_id); contradiction_links_[item.edge_id].push_back(edge.edge_id); }
        edges_.push_back(edge); if (!edge.contradiction_links.empty()) contradiction_links_[edge.edge_id] = edge.contradiction_links; return edge.edge_id;
    }

    bool has_node(uint64_t node_id) const { return nodes_.find(node_id) != nodes_.end(); }
    const BeliefNode* node(uint64_t node_id) const { const auto iterator = nodes_.find(node_id); return iterator == nodes_.end() ? nullptr : &iterator->second; }
    const BeliefEdge* edge(uint64_t edge_id) const { for (const auto& edge_item : edges_) if (edge_item.edge_id == edge_id) return &edge_item; return nullptr; }
    const std::vector<BeliefEdge>& edges() const { return edges_; }
    std::vector<const BeliefEdge*> find(uint64_t subject_id, uint64_t predicate_id, std::optional<uint64_t> object_id = std::nullopt) const { std::vector<const BeliefEdge*> result; for (const auto& edge : edges_) if (edge.subject_id == subject_id && edge.predicate_id == predicate_id && (!object_id || edge.object_id == *object_id)) result.push_back(&edge); return result; }
    std::vector<uint64_t> contradictions(uint64_t edge_id) const { const auto iterator = contradiction_links_.find(edge_id); return iterator == contradiction_links_.end() ? std::vector<uint64_t>{} : iterator->second; }
    uint64_t revision() const { return revision_; }
    size_t contradiction_count() const { return contradiction_links_.size(); }

    uint64_t state_hash() const {
        uint64_t hash = 1469598103934665603ULL;
        for (const auto& item : nodes_) hash ^= item.first + (static_cast<uint64_t>(item.second.type_id) << 32) + (hash << 6) + (hash >> 2);
        for (const auto& edge : edges_) hash ^= edge.edge_id + edge.subject_id + edge.predicate_id + edge.object_id + (hash << 6) + (hash >> 2);
        for (const auto& item : contradiction_links_) hash ^= item.first + item.second.size() + (hash << 6) + (hash >> 2);
        return hash;
    }

private:
    uint64_t revision_ = 0;
    uint64_t next_edge_id_ = 1;
    std::map<uint64_t, BeliefNode> nodes_;
    std::vector<BeliefEdge> edges_;
    std::map<uint64_t, std::vector<uint64_t>> contradiction_links_;
};

enum class ContradictionMode : uint32_t { PREFER_RELIABLE = 0, SEEK_EVIDENCE = 1, ABSTAIN = 2 };
struct ContradictionDecision { ContradictionMode mode = ContradictionMode::ABSTAIN; uint64_t selected_edge_id = 0; std::vector<uint64_t> conflicting_edge_ids; std::string rationale; };

class ContradictionManager {
public:
    ContradictionDecision resolve(const BeliefGraph& graph, uint64_t subject_id, uint64_t predicate_id, float margin = 0.20f) const {
        const auto edges = graph.find(subject_id, predicate_id);
        ContradictionDecision decision; for (const auto* edge : edges) decision.conflicting_edge_ids.push_back(edge->edge_id);
        if (edges.size() < 2) { decision.mode = edges.empty() ? ContradictionMode::ABSTAIN : ContradictionMode::PREFER_RELIABLE; decision.selected_edge_id = edges.empty() ? 0 : edges.front()->edge_id; decision.rationale = "no-conflict"; return decision; }
        auto reliability = [](const BeliefEdge* edge) { if (edge->evidence.empty()) return edge->confidence; float total = 0.0f; for (const auto& evidence : edge->evidence) total += evidence.reliability; return total / edge->evidence.size(); };
        const auto best = std::max_element(edges.begin(), edges.end(), [&](const auto* a, const auto* b) { const float ar = reliability(a), br = reliability(b); return ar == br ? a->edge_id > b->edge_id : ar < br; });
        const auto second = std::max_element(edges.begin(), edges.end(), [&](const auto* a, const auto* b) { if (a == *best) return true; if (b == *best) return false; const float ar = reliability(a), br = reliability(b); return ar == br ? a->edge_id > b->edge_id : ar < br; });
        if (second != edges.end() && reliability(*best) - reliability(*second) > margin) { decision.mode = ContradictionMode::PREFER_RELIABLE; decision.selected_edge_id = (*best)->edge_id; decision.rationale = "reliability-margin"; } else { decision.mode = ContradictionMode::SEEK_EVIDENCE; decision.rationale = "posterior-margin-small"; }
        return decision;
    }
};

struct ReasoningQuery {
    uint64_t subject_id = 0;
    uint64_t predicate_id = 0;
    std::optional<uint64_t> object_id;
    uint32_t subject_type = 0;
    uint32_t object_type = 0;
};

struct ProofStep {
    uint64_t rule_id = 0;
    std::vector<uint64_t> premise_edge_ids;
    std::map<std::string, uint64_t> substitutions;
    std::string type_check;
};

struct ProofResult {
    ReasoningStatus status = ReasoningStatus::UNKNOWN;
    ReasoningQuery query;
    std::vector<uint64_t> premise_edge_ids;
    std::vector<ProofStep> proof;
    std::vector<EvidenceRef> evidence;
    uint64_t result_revision = 0;
    std::string explanation;
    bool unsupported = true;
};

struct DeductionRule {
    uint64_t rule_id = 0;
    uint64_t premise_predicate = 0;
    uint64_t conclusion_predicate = 0;
    uint32_t subject_type = 0;
    uint32_t premise_object_type = 0;
    uint32_t conclusion_object_type = 0;
    float confidence = 0.0f;
};

struct EpisodeSet {
    std::vector<BeliefEdge> observations;
    std::vector<BeliefEdge> exceptions;
};

struct Hypothesis {
    uint64_t hypothesis_id = 0;
    uint64_t predicate_id = 0;
    uint32_t subject_type = 0;
    uint32_t object_type = 0;
    uint32_t support = 0;
    uint32_t exceptions = 0;
    float confidence = 0.0f;
    float complexity_penalty = 0.0f;
    bool promoted = false;
};

struct ExplanationCandidate {
    uint64_t explanation_id = 0;
    std::string description;
    float fit = 0.0f;
    float prior = 0.0f;
    float complexity = 0.0f;
    float contradiction_penalty = 0.0f;
    float posterior = 0.0f;
};

struct Explanation {
    ExplanationCandidate selected;
    std::vector<ExplanationCandidate> alternatives;
    std::vector<EvidenceRef> evidence;
    bool alternatives_required = false;
};

struct AnalogyNode { uint64_t node_id = 0; std::string name; uint32_t type_id = 0; };
struct AnalogyRelation { uint64_t subject_id = 0; uint64_t predicate_id = 0; uint64_t object_id = 0; };
struct RelationalDomain { std::vector<AnalogyNode> nodes; std::vector<AnalogyRelation> relations; };
struct AnalogyResult { std::map<uint64_t, uint64_t> mapping; float structural_accuracy = 0.0f; float surface_similarity = 0.0f; bool false_positive = false; };

class ReasoningEngine {
public:
    void add_rule(const DeductionRule& rule) { if (rule.rule_id == 0 || rule.premise_predicate == 0 || rule.conclusion_predicate == 0) throw std::invalid_argument("deduction rule is invalid"); rules_.push_back(rule); }

    ProofResult deduce(const ReasoningQuery& query, const BeliefGraph& graph) const {
        ProofResult result; result.query = query; result.result_revision = graph.revision();
        const auto direct = graph.find(query.subject_id, query.predicate_id, query.object_id);
        if (!direct.empty()) { const auto* edge = direct.front(); result.status = ReasoningStatus::VALID; result.unsupported = false; result.premise_edge_ids.push_back(edge->edge_id); result.evidence = edge->evidence; result.explanation = "direct-evidence"; return result; }
        const BeliefNode* subject = graph.node(query.subject_id); if (!subject) { result.status = ReasoningStatus::UNKNOWN; result.explanation = "subject-not-found"; return result; }
        for (const auto& rule : rules_) {
            if (rule.conclusion_predicate != query.predicate_id || subject->type_id != rule.subject_type || (query.object_id.has_value() && rule.conclusion_object_type != query.object_type)) continue;
            for (const auto* premise : graph.find(query.subject_id, rule.premise_predicate)) {
                const auto* object = graph.node(premise->object_id); if (!object || object->type_id != rule.premise_object_type || (query.object_id && premise->object_id != *query.object_id)) continue;
                ProofStep step; step.rule_id = rule.rule_id; step.premise_edge_ids = {premise->edge_id}; step.substitutions["subject"] = query.subject_id; step.substitutions["object"] = premise->object_id; step.type_check = "valid";
                result.status = ReasoningStatus::VALID; result.unsupported = false; result.premise_edge_ids = {premise->edge_id}; result.proof.push_back(step); result.evidence = premise->evidence; result.explanation = "typed-rule"; return result;
            }
        }
        result.status = ReasoningStatus::UNKNOWN; result.explanation = "no-supported-proof"; return result;
    }

    std::vector<Hypothesis> induce(const EpisodeSet& episodes) const {
        std::map<std::tuple<uint64_t, uint32_t, uint32_t>, Hypothesis> table;
        for (const auto& edge : episodes.observations) { const auto key = std::make_tuple(edge.predicate_id, static_cast<uint32_t>(edge.subject_id % 1000 + 1), static_cast<uint32_t>(edge.object_id % 1000 + 1)); auto& hypothesis = table[key]; hypothesis.hypothesis_id = language_mix(edge.predicate_id ^ edge.subject_id ^ edge.object_id); hypothesis.predicate_id = edge.predicate_id; hypothesis.subject_type = std::get<1>(key); hypothesis.object_type = std::get<2>(key); ++hypothesis.support; }
        for (const auto& edge : episodes.exceptions) for (auto& item : table) if (item.second.predicate_id == edge.predicate_id) ++item.second.exceptions;
        std::vector<Hypothesis> result; for (auto& item : table) { auto& hypothesis = item.second; hypothesis.complexity_penalty = 0.05f + 0.01f * static_cast<float>(hypothesis.subject_type != hypothesis.object_type); hypothesis.confidence = static_cast<float>(hypothesis.support) / static_cast<float>(hypothesis.support + hypothesis.exceptions + 1); hypothesis.promoted = hypothesis.support >= 2 && hypothesis.confidence >= 0.8f; result.push_back(hypothesis); }
        std::sort(result.begin(), result.end(), [](const Hypothesis& a, const Hypothesis& b) { const float as = a.confidence - a.complexity_penalty; const float bs = b.confidence - b.complexity_penalty; if (as != bs) return as > bs; return a.complexity_penalty < b.complexity_penalty; }); return result;
    }

    void add_abductive_cause(const ExplanationCandidate& candidate) { if (candidate.explanation_id == 0) throw std::invalid_argument("abductive cause ID is invalid"); causes_.push_back(candidate); }
    std::vector<Explanation> explain(const BeliefEdge& observation, const BeliefGraph& graph) const {
        (void)graph; if (causes_.empty()) return {};
        std::vector<ExplanationCandidate> ranked = causes_; for (auto& candidate : ranked) candidate.posterior = candidate.fit + candidate.prior - candidate.complexity - candidate.contradiction_penalty;
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { if (a.posterior != b.posterior) return a.posterior > b.posterior; return a.explanation_id < b.explanation_id; });
        Explanation explanation; explanation.selected = ranked.front(); if (ranked.size() > 1 && ranked[0].posterior - ranked[1].posterior <= 0.15f) { explanation.alternatives_required = true; explanation.alternatives.assign(ranked.begin() + 1, ranked.end()); } explanation.evidence = observation.evidence; return {explanation};
    }

    AnalogyResult map_structure(const RelationalDomain& source, const RelationalDomain& target) const {
        AnalogyResult result; std::map<uint32_t, std::vector<uint64_t>> target_by_type; for (const auto& node : target.nodes) target_by_type[node.type_id].push_back(node.node_id);
        size_t matched = 0; for (const auto& source_node : source.nodes) { const auto candidates = target_by_type[source_node.type_id]; if (candidates.size() == 1) result.mapping[source_node.node_id] = candidates.front(); }
        for (const auto& relation : source.relations) { const auto a = result.mapping.find(relation.subject_id); const auto b = result.mapping.find(relation.object_id); if (a == result.mapping.end() || b == result.mapping.end()) continue; for (const auto& target_relation : target.relations) if (target_relation.subject_id == a->second && target_relation.object_id == b->second && target_relation.predicate_id == relation.predicate_id) { ++matched; break; } }
        result.structural_accuracy = source.relations.empty() ? 0.0f : static_cast<float>(matched) / static_cast<float>(source.relations.size());
        size_t surface_matches = 0; for (const auto& source_node : source.nodes) for (const auto& target_node : target.nodes) if (source_node.name == target_node.name) ++surface_matches;
        result.surface_similarity = source.nodes.empty() ? 0.0f : static_cast<float>(surface_matches) / static_cast<float>(source.nodes.size());
        result.false_positive = result.structural_accuracy < 0.5f && result.surface_similarity > 0.5f;
        return result;
    }

private:
    std::vector<DeductionRule> rules_;
    std::vector<ExplanationCandidate> causes_;
};

struct CausalVariable { uint64_t id = 0; uint32_t domain_type = 0; std::vector<uint32_t> values; };
struct ConditionalTable { bool or_function = false; std::map<std::string, uint32_t> rows; };
struct CausalMechanism { uint64_t target = 0; std::vector<uint64_t> parents; ConditionalTable table; float confidence = 0.0f; };
struct CausalWorldState { std::map<uint64_t, uint32_t> values; };
struct CausalQuery { uint64_t variable_id = 0; };
struct Intervention { uint64_t variable_id = 0; uint32_t value = 0; uint64_t query_variable = 0; };
struct CausalQueryResult { ReasoningStatus status = ReasoningStatus::UNKNOWN; uint32_t value = 0; uint64_t model_revision = 0; std::vector<Intervention> interventions; std::vector<EvidenceRef> evidence; size_t live_mutation_count = 0; std::string explanation; };

class CausalEngine {
public:
    void add_variable(const CausalVariable& variable, uint32_t initial_value = 0) { if (variable.id == 0 || variable.values.empty() || std::find(variable.values.begin(), variable.values.end(), initial_value) == variable.values.end()) throw std::invalid_argument("causal variable is invalid"); variables_[variable.id] = variable; live_state_.values[variable.id] = initial_value; ++revision_; }
    void add_mechanism(const CausalMechanism& mechanism) { if (variables_.find(mechanism.target) == variables_.end() || mechanism.parents.empty()) throw std::invalid_argument("causal mechanism is invalid"); mechanisms_[mechanism.target] = mechanism; ++revision_; }
    void add_evidence(uint64_t variable_id, const EvidenceRef& evidence) { evidence_[variable_id].push_back(evidence); }
    CausalQueryResult observe(const CausalQuery& query) const { CausalQueryResult result; result.model_revision = revision_; const auto iterator = live_state_.values.find(query.variable_id); if (iterator == live_state_.values.end()) { result.status = ReasoningStatus::OUT_OF_SCOPE; result.explanation = "variable-out-of-scope"; return result; } result.status = ReasoningStatus::VALID; result.value = iterator->second; auto evidence_iterator = evidence_.find(query.variable_id); if (evidence_iterator != evidence_.end()) result.evidence = evidence_iterator->second; result.explanation = "observation"; return result; }
    CausalQueryResult intervene(const Intervention& intervention) const { return evaluate_branch(live_state_, intervention, true); }
    CausalQueryResult counterfactual(const CausalWorldState& world, const Intervention& intervention) const { return evaluate_branch(world, intervention, false); }
    const CausalWorldState& live_state() const { return live_state_; }
    uint64_t revision() const { return revision_; }

private:
    static std::string key_for(const std::vector<uint32_t>& values) { std::ostringstream stream; for (size_t i = 0; i < values.size(); ++i) { if (i) stream << ','; stream << values[i]; } return stream.str(); }
    CausalQueryResult evaluate_branch(const CausalWorldState& input, const Intervention& intervention, bool intervention_mode) const {
        CausalQueryResult result; result.model_revision = revision_; result.interventions.push_back(intervention); CausalWorldState branch = input; if (variables_.find(intervention.variable_id) == variables_.end()) { result.status = ReasoningStatus::OUT_OF_SCOPE; result.explanation = "intervention-variable-out-of-scope"; return result; }
        if (std::find(variables_.at(intervention.variable_id).values.begin(), variables_.at(intervention.variable_id).values.end(), intervention.value) == variables_.at(intervention.variable_id).values.end()) { result.status = ReasoningStatus::INVALID; result.explanation = "intervention-value-invalid"; return result; }
        branch.values[intervention.variable_id] = intervention.value;
        for (const auto& mechanism_item : mechanisms_) { const auto& mechanism = mechanism_item.second; std::vector<uint32_t> parent_values; bool missing = false; for (uint64_t parent : mechanism.parents) { const auto iterator = branch.values.find(parent); if (iterator == branch.values.end()) { missing = true; break; } parent_values.push_back(iterator->second); } if (missing) { result.status = ReasoningStatus::OUT_OF_SCOPE; result.explanation = "mechanism-parent-missing"; return result; }
            uint32_t output = 0; if (mechanism.table.or_function) output = std::any_of(parent_values.begin(), parent_values.end(), [](uint32_t value) { return value != 0; }) ? 1U : 0U; else { const auto row = mechanism.table.rows.find(key_for(parent_values)); if (row == mechanism.table.rows.end()) { result.status = ReasoningStatus::UNKNOWN; result.explanation = "mechanism-row-missing"; return result; } output = row->second; } branch.values[mechanism.target] = output; }
        const uint64_t queried_variable = intervention.query_variable == 0 ? intervention.variable_id : intervention.query_variable;
        const auto value = branch.values.find(queried_variable); if (value == branch.values.end()) { result.status = ReasoningStatus::OUT_OF_SCOPE; result.explanation = "query-variable-out-of-scope"; return result; }
        result.status = ReasoningStatus::VALID; result.value = value->second; auto evidence_iterator = evidence_.find(queried_variable); if (evidence_iterator != evidence_.end()) result.evidence = evidence_iterator->second; result.live_mutation_count = intervention_mode ? 0 : 0; result.explanation = intervention_mode ? "do-branch" : "counterfactual-branch"; return result;
    }

    uint64_t revision_ = 0;
    std::map<uint64_t, CausalVariable> variables_;
    std::map<uint64_t, CausalMechanism> mechanisms_;
    CausalWorldState live_state_;
    std::map<uint64_t, std::vector<EvidenceRef>> evidence_;
};

struct ResourceLimits { size_t max_nodes = 10000; uint32_t max_depth = 32; uint64_t max_time_ms = 100; size_t max_memory_bytes = 2U * 1024U * 1024U; };

class ResourceGuard {
public:
    explicit ResourceGuard(const ResourceLimits& limits = ResourceLimits{}) : limits_(limits) {}
    bool consume_node(size_t count = 1) { if (nodes_ + count > limits_.max_nodes) return false; nodes_ += count; return true; }
    bool enter_depth(uint32_t depth) { if (depth > limits_.max_depth) return false; max_depth_seen_ = std::max(max_depth_seen_, depth); return true; }
    bool consume_time(uint64_t milliseconds) { if (time_ms_ + milliseconds > limits_.max_time_ms) return false; time_ms_ += milliseconds; return true; }
    bool consume_memory(size_t bytes) { if (memory_bytes_ + bytes > limits_.max_memory_bytes) return false; memory_bytes_ += bytes; return true; }
    size_t nodes() const { return nodes_; }
    uint32_t max_depth_seen() const { return max_depth_seen_; }
    uint64_t time_ms() const { return time_ms_; }
    size_t memory_bytes() const { return memory_bytes_; }
    const ResourceLimits& limits() const { return limits_; }

private:
    ResourceLimits limits_;
    size_t nodes_ = 0, memory_bytes_ = 0;
    uint32_t max_depth_seen_ = 0;
    uint64_t time_ms_ = 0;
};

struct ExecutiveGoal { uint64_t goal_id = 0; uint64_t target_variable = 0; uint32_t desired_value = 0; float urgency = 0.0f; };
struct ExecutiveOption { uint64_t option_id = 0; uint64_t goal_id = 0; ActionCommand action; Intervention intervention; float expected_utility = 0.0f; float information_gain = 0.0f; float uncertainty = 0.0f; float resource_cost = 0.0f; float risk = 0.0f; uint32_t horizon = 0; };
struct ExecutiveDecision { ExecutiveMode mode = ExecutiveMode::ABSTAIN; uint64_t selected_option_id = 0; float score = 0.0f; float expected_utility = 0.0f; float information_gain = 0.0f; float uncertainty = 0.0f; float resource_cost = 0.0f; float risk = 0.0f; std::string rationale; bool abstained = true; };
struct CognitiveState { CausalWorldState world; float uncertainty = 0.0f; bool request_ambiguous = false; bool unsafe_request = false; };

class ExecutiveController {
public:
    explicit ExecutiveController(float lambda_info = 1.0f, float lambda_risk = 1.0f, float lambda_cost = 1.0f, float lambda_uncertainty = 1.0f) : lambda_info_(lambda_info), lambda_risk_(lambda_risk), lambda_cost_(lambda_cost), lambda_uncertainty_(lambda_uncertainty) {}
    void add_option_template(const ExecutiveOption& option) { if (option.option_id == 0) throw std::invalid_argument("executive option ID is invalid"); options_.push_back(option); }
    std::vector<ExecutiveOption> generate_options(const ExecutiveGoal& goal, const CognitiveState& state) const { (void)state; std::vector<ExecutiveOption> result; for (const auto& option : options_) if (option.goal_id == goal.goal_id) result.push_back(option); return result; }
    ExecutiveDecision select(const std::vector<ExecutiveOption>& options, bool ambiguous = false) const {
        ExecutiveDecision decision; if (options.empty()) { decision.mode = ExecutiveMode::ABSTAIN; decision.rationale = "no-options"; return decision; }
        if (ambiguous) { const auto best_info = std::max_element(options.begin(), options.end(), [](const auto& a, const auto& b) { return a.information_gain < b.information_gain; }); if (best_info->information_gain > best_info->resource_cost) { decision.mode = ExecutiveMode::ASK; decision.selected_option_id = best_info->option_id; decision.information_gain = best_info->information_gain; decision.resource_cost = best_info->resource_cost; decision.uncertainty = best_info->uncertainty; decision.rationale = "clarification-reduces-uncertainty"; decision.abstained = false; return decision; } }
        const ExecutiveOption* best = nullptr; float best_score = -std::numeric_limits<float>::infinity();
        for (const auto& option : options) { const float score = option.expected_utility + lambda_info_ * option.information_gain - lambda_risk_ * option.risk - lambda_cost_ * option.resource_cost - lambda_uncertainty_ * option.uncertainty; if (!best || score > best_score || (score == best_score && option.option_id < best->option_id)) { best = &option; best_score = score; } }
        if (best->risk >= 0.95f || best->uncertainty >= 0.95f) { decision.mode = ExecutiveMode::ABSTAIN; decision.rationale = "risk-or-uncertainty-too-high"; return decision; }
        decision.mode = best->action.safe ? ExecutiveMode::ACT : ExecutiveMode::SIMULATE; decision.selected_option_id = best->option_id; decision.score = best_score; decision.expected_utility = best->expected_utility; decision.information_gain = best->information_gain; decision.uncertainty = best->uncertainty; decision.resource_cost = best->resource_cost; decision.risk = best->risk; decision.rationale = "utility-policy"; decision.abstained = false; return decision;
    }
    void record_outcome(const ExecutiveDecision& decision, bool correct) { outcomes_.push_back({decision, correct}); }
    const std::vector<std::pair<ExecutiveDecision, bool>>& outcomes() const { return outcomes_; }

private:
    float lambda_info_ = 1.0f, lambda_risk_ = 1.0f, lambda_cost_ = 1.0f, lambda_uncertainty_ = 1.0f;
    std::vector<ExecutiveOption> options_;
    std::vector<std::pair<ExecutiveDecision, bool>> outcomes_;
};

struct ReasoningResult { ReasoningStatus status = ReasoningStatus::UNKNOWN; float confidence = 0.0f; bool supported = false; uint64_t revision = 0; std::vector<EvidenceRef> evidence; };
struct EvidenceContext { uint32_t evidence_count = 0; float reliability = 0.0f; float novelty = 1.0f; float uncertainty = 1.0f; bool unsupported = true; float precision = 0.0f; float raw_activation = 0.0f; };
struct ConfidenceEstimate { float confidence = 0.0f; bool supported = false; bool calibrated = false; };
struct RiskProfile { float risk_tolerance = 0.5f; float minimum_confidence = 0.6f; bool require_support = true; };
struct OutcomeLogEntry { float confidence = 0.0f; bool correct = false; bool abstained = false; bool supported = false; };
struct CalibrationReport { float expected_calibration_error = 1.0f; float brier_score = 1.0f; float selective_accuracy = 0.0f; float coverage = 0.0f; size_t bucket_count = 0; };

class MetacognitiveMonitor {
public:
    ConfidenceEstimate estimate(const ReasoningResult& result, const EvidenceContext& context) const {
        ConfidenceEstimate estimate; estimate.supported = result.supported && !context.unsupported;
        const float evidence_term = static_cast<float>(context.evidence_count) / static_cast<float>(context.evidence_count + 2U);
        estimate.confidence = std::max(0.0f, std::min(1.0f, 0.35f * context.reliability + 0.30f * evidence_term + 0.20f * (1.0f - context.novelty) + 0.15f * (1.0f - context.uncertainty)));
        if (!estimate.supported) estimate.confidence = std::min(estimate.confidence, 0.25f);
        return estimate;
    }
    bool should_abstain(const ConfidenceEstimate& confidence, const RiskProfile& risk) const { return (risk.require_support && !confidence.supported) || confidence.confidence < risk.minimum_confidence || risk.risk_tolerance * (1.0f - confidence.confidence) < 0.0f; }
    void record(const ConfidenceEstimate& confidence, bool correct, bool abstained) { outcomes_.push_back({confidence.confidence, correct, abstained, confidence.supported}); }
    CalibrationReport evaluate() const {
        CalibrationReport report; if (outcomes_.empty()) return report; const size_t buckets = 10; report.bucket_count = buckets; float ece = 0.0f, brier = 0.0f; size_t covered = 0, covered_correct = 0;
        for (size_t bucket = 0; bucket < buckets; ++bucket) { std::vector<const OutcomeLogEntry*> group; const float lower = static_cast<float>(bucket) / buckets, upper = static_cast<float>(bucket + 1) / buckets; for (const auto& outcome : outcomes_) if (outcome.confidence >= lower && (outcome.confidence < upper || bucket + 1 == buckets)) group.push_back(&outcome); if (group.empty()) continue; const float avg_confidence = std::accumulate(group.begin(), group.end(), 0.0f, [](float total, const auto* item) { return total + item->confidence; }) / group.size(); const float accuracy = static_cast<float>(std::count_if(group.begin(), group.end(), [](const auto* item) { return item->correct; })) / group.size(); ece += static_cast<float>(group.size()) / outcomes_.size() * std::fabs(avg_confidence - accuracy); }
        for (const auto& outcome : outcomes_) { brier += (outcome.confidence - (outcome.correct ? 1.0f : 0.0f)) * (outcome.confidence - (outcome.correct ? 1.0f : 0.0f)); if (!outcome.abstained) { ++covered; if (outcome.correct) ++covered_correct; } }
        report.expected_calibration_error = ece; report.brier_score = brier / outcomes_.size(); report.coverage = static_cast<float>(covered) / outcomes_.size(); report.selective_accuracy = covered == 0 ? 0.0f : static_cast<float>(covered_correct) / covered; return report;
    }
    void clear() { outcomes_.clear(); }

private:
    std::vector<OutcomeLogEntry> outcomes_;
};

} // namespace genesis
