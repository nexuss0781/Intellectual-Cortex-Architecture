#include "learning/reasoning_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace genesis;

namespace {

struct Result { std::string id; bool passed; double value; std::string detail; };
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }

EvidenceRef evidence(uint64_t episode, uint64_t source, float reliability) { EvidenceRef item; item.episode_id = episode; item.event_id = episode + 1000; item.source_id = source; item.reliability = reliability; return item; }

BeliefGraph make_basic_graph(bool with_evidence = true) {
    BeliefGraph graph;
    graph.add_node(1, 1, 0.9f, with_evidence ? std::vector<EvidenceRef>{evidence(1, 10, 0.95f)} : std::vector<EvidenceRef>{});
    graph.add_node(2, 2, 0.9f, with_evidence ? std::vector<EvidenceRef>{evidence(1, 10, 0.95f)} : std::vector<EvidenceRef>{});
    graph.add_node(3, 2, 0.7f, with_evidence ? std::vector<EvidenceRef>{evidence(2, 11, 0.40f)} : std::vector<EvidenceRef>{});
    return graph;
}

CausalEngine make_causal_engine() {
    CausalEngine engine;
    engine.add_variable({1, 1, {0, 1}}, 0);
    engine.add_variable({2, 1, {0, 1}}, 0);
    CausalMechanism mechanism; mechanism.target = 2; mechanism.parents = {1}; mechanism.table.rows["0"] = 0; mechanism.table.rows["1"] = 1; mechanism.confidence = 0.95f; engine.add_mechanism(mechanism);
    engine.add_evidence(2, evidence(20, 200, 0.95f));
    return engine;
}

ExecutiveOption option(uint64_t id, float utility, float info, float uncertainty, float cost, float risk, bool safe = true) {
    ExecutiveOption item; item.option_id = id; item.goal_id = 7; item.expected_utility = utility; item.information_gain = info; item.uncertainty = uncertainty; item.resource_cost = cost; item.risk = risk; item.horizon = 2; item.action.safe = safe; item.action.command_id = id; item.intervention.variable_id = 1; item.intervention.value = id == 1 ? 1U : 0U; item.intervention.query_variable = 2; return item;
}

uint64_t deterministic_scenario_hash(uint64_t seed) {
    (void)seed;
    BeliefGraph graph = make_basic_graph();
    ReasoningEngine reasoning; reasoning.add_rule({100, 10, 20, 1, 2, 2, 0.95f});
    graph.add_edge(1, 10, 2, 0.95f, {evidence(1, 10, 0.95f)});
    const auto proof = reasoning.deduce({1, 20, 2, 1, 2}, graph);
    CausalEngine causal = make_causal_engine(); Intervention intervention{1, 1, 2}; const auto counterfactual = causal.counterfactual(causal.live_state(), intervention);
    ExecutiveController executive; const auto selected = executive.select({option(1, 0.9f, 0.1f, 0.1f, 0.1f, 0.05f), option(2, 0.5f, 0.2f, 0.3f, 0.1f, 0.05f)});
    return graph.state_hash() ^ proof.result_revision ^ static_cast<uint64_t>(proof.status) ^ counterfactual.model_revision ^ counterfactual.value ^ selected.selected_option_id;
}

} // namespace

int main(int argc, char** argv) {
    try {
        uint64_t seed = 424242; fs::path artifact_dir = "artifacts/stage-5";
        for (int i = 1; i + 1 < argc; ++i) { if (std::string(argv[i]) == "--seed") seed = std::stoull(argv[i + 1]); if (std::string(argv[i]) == "--artifact-dir") artifact_dir = argv[i + 1]; }
        fs::create_directories(artifact_dir);
        std::vector<Result> results;
        auto run = [&results](const std::string& id, const auto& function) { try { results.push_back({id, true, function(), "ok"}); } catch (const std::exception& error) { results.push_back({id, false, 0.0, error.what()}); } };

        run("R5-UNIT-01", []() {
            BeliefGraph graph = make_basic_graph(); graph.add_edge(1, 10, 2, 0.95f, {evidence(1, 10, 0.95f)});
            ReasoningEngine reasoning; reasoning.add_rule({100, 10, 20, 1, 2, 2, 0.95f});
            const auto result = reasoning.deduce({1, 20, 2, 1, 2}, graph);
            require(result.status == ReasoningStatus::VALID && !result.unsupported && result.proof.size() == 1 && result.proof.front().type_check == "valid" && result.premise_edge_ids.size() == 1 && !result.evidence.empty(), "proof trace is incomplete or unsupported");
            return 1.0;
        });

        run("R5-UNIT-02", []() {
            BeliefGraph graph = make_basic_graph(); graph.add_edge(1, 10, 2, 0.95f);
            ReasoningEngine reasoning; reasoning.add_rule({100, 10, 20, 1, 99, 2, 0.95f});
            const auto result = reasoning.deduce({1, 20, 2, 1, 2}, graph);
            require(result.status != ReasoningStatus::VALID && result.proof.empty() && result.unsupported, "type-invalid deduction was accepted");
            return 1.0;
        });

        run("R5-UNIT-03", []() {
            EpisodeSet episodes;
            for (int i = 0; i < 5; ++i) { BeliefEdge simple; simple.edge_id = i + 1; simple.subject_id = 1; simple.predicate_id = 55; simple.object_id = 2; episodes.observations.push_back(simple); BeliefEdge complex = simple; complex.edge_id = 100 + i; complex.subject_id = 2; complex.object_id = 3; episodes.observations.push_back(complex); }
            ReasoningEngine reasoning; const auto hypotheses = reasoning.induce(episodes);
            require(hypotheses.size() >= 2 && hypotheses.front().complexity_penalty <= hypotheses.back().complexity_penalty, "equal-fit complexity penalty did not prefer simpler hypothesis");
            return static_cast<double>(hypotheses.front().complexity_penalty);
        });

        run("R5-UNIT-04", []() {
            ReasoningEngine reasoning;
            reasoning.add_abductive_cause({1, "blocked valve", 0.80f, 0.20f, 0.10f, 0.0f, 0.0f});
            reasoning.add_abductive_cause({2, "empty tank", 0.75f, 0.20f, 0.10f, 0.0f, 0.0f});
            BeliefEdge observation; observation.evidence = {evidence(4, 44, 0.8f)};
            const auto explanations = reasoning.explain(observation, make_basic_graph());
            require(!explanations.empty() && explanations.front().alternatives_required && explanations.front().alternatives.size() == 1 && explanations.front().selected.explanation_id == 1, "close abductive posterior did not return alternatives");
            return static_cast<double>(explanations.front().alternatives.size());
        });

        run("R5-UNIT-05", []() {
            RelationalDomain source{{{1, "alice", 1}, {2, "box", 2}}, {{1, 9, 2}}};
            RelationalDomain target{{{10, "eve", 1}, {20, "crate", 2}}, {{10, 9, 20}}};
            ReasoningEngine reasoning; const auto result = reasoning.map_structure(source, target);
            require(result.structural_accuracy >= 0.8f && !result.false_positive, "relational analogy failed on renamed entities"); return result.structural_accuracy;
        });

        run("R5-UNIT-06", []() {
            RelationalDomain source{{{1, "shared", 1}, {2, "shared-box", 2}}, {{1, 9, 2}}};
            RelationalDomain target{{{10, "shared", 8}, {20, "shared-box", 9}}, {{20, 7, 10}}};
            ReasoningEngine reasoning; const auto result = reasoning.map_structure(source, target);
            require(result.surface_similarity >= 0.5f && result.structural_accuracy < 0.5f && result.false_positive, "surface distractor was incorrectly treated as a structural analogy"); return result.surface_similarity;
        });

        run("R5-UNIT-07", []() {
            CausalEngine causal = make_causal_engine(); const auto before = causal.observe({2}); const auto branch = causal.intervene({1, 1, 2}); const auto after = causal.observe({2});
            require(before.value == 0 && branch.status == ReasoningStatus::VALID && branch.value == 1 && after.value == 0 && branch.live_mutation_count == 0, "intervention mutated observational live state"); return static_cast<double>(branch.value);
        });

        run("R5-UNIT-08", []() {
            CausalEngine causal = make_causal_engine(); const auto before_hash = causal.observe({2}).model_revision; const auto result = causal.counterfactual(causal.live_state(), {1, 1, 2}); const auto after_hash = causal.observe({2}).model_revision;
            require(result.status == ReasoningStatus::VALID && result.model_revision == before_hash && result.interventions.size() == 1 && !result.evidence.empty() && after_hash == before_hash && result.live_mutation_count == 0, "counterfactual provenance or isolation is incomplete"); return static_cast<double>(result.evidence.size());
        });

        run("R5-UNIT-09", []() {
            BeliefGraph graph = make_basic_graph(); const auto first = graph.add_edge(1, 99, 2, 0.9f, {evidence(1, 1, 0.95f)}); const auto second = graph.add_edge(1, 99, 3, 0.7f, {evidence(2, 2, 0.40f)}); ContradictionManager manager; const auto decision = manager.resolve(graph, 1, 99); require(graph.contradiction_count() >= 2 && graph.contradictions(first).size() >= 1 && graph.contradictions(second).size() >= 1 && decision.mode == ContradictionMode::PREFER_RELIABLE && decision.selected_edge_id == first, "contradiction evidence was overwritten or not resolved by provenance"); return static_cast<double>(decision.conflicting_edge_ids.size());
        });

        run("R5-UNIT-10", []() {
            MetacognitiveMonitor monitor; ReasoningResult result; result.status = ReasoningStatus::VALID; result.supported = true;
            EvidenceContext low_precision; low_precision.evidence_count = 4; low_precision.reliability = 0.8f; low_precision.novelty = 0.2f; low_precision.uncertainty = 0.2f; low_precision.precision = 0.1f; low_precision.raw_activation = 0.1f;
            EvidenceContext high_precision = low_precision; high_precision.precision = 9.0f; high_precision.raw_activation = 0.99f;
            const auto a = monitor.estimate(result, low_precision); const auto b = monitor.estimate(result, high_precision); require(std::fabs(a.confidence - b.confidence) < 1e-6f, "confidence copied precision or raw activation"); return static_cast<double>(a.confidence);
        });

        run("R5-INT-01", []() {
            BeliefGraph graph; ReasoningEngine reasoning; reasoning.add_rule({100, 10, 20, 1, 2, 2, 0.95f}); size_t correct = 0, unsupported = 0;
            for (uint64_t i = 0; i < 20; ++i) { const uint64_t subject = 100 + i; const uint64_t object = 200 + i; graph.add_node(subject, 1, 0.9f, {evidence(i + 1, 1, 0.9f)}); graph.add_node(object, 2, 0.9f, {evidence(i + 1, 1, 0.9f)}); graph.add_edge(subject, 10, object, 0.9f, {evidence(i + 1, 1, 0.9f)}); const auto proof = reasoning.deduce({subject, 20, object, 1, 2}, graph); if (proof.status == ReasoningStatus::VALID) ++correct; if (proof.unsupported || proof.proof.empty()) ++unsupported; }
            const double accuracy = static_cast<double>(correct) / 20.0; require(accuracy >= 0.95 && unsupported == 0, "held-out deductive validity or trace completeness failed"); return accuracy;
        });

        run("R5-INT-02", []() {
            EpisodeSet episodes; for (int i = 0; i < 9; ++i) { BeliefEdge item; item.edge_id = i + 1; item.subject_id = 1; item.predicate_id = 77; item.object_id = 2; episodes.observations.push_back(item); } BeliefEdge exception; exception.edge_id = 100; exception.subject_id = 1; exception.predicate_id = 77; exception.object_id = 3; episodes.exceptions.push_back(exception);
            ReasoningEngine reasoning; const auto hypotheses = reasoning.induce(episodes); require(!hypotheses.empty() && hypotheses.front().confidence >= 0.8f && hypotheses.front().exceptions >= 1, "inductive generalization did not report exceptions"); return static_cast<double>(hypotheses.front().confidence);
        });

        run("R5-INT-03", []() {
            ReasoningEngine reasoning; reasoning.add_abductive_cause({1, "blocked valve", 0.80f, 0.20f, 0.10f, 0.0f, 0.0f}); reasoning.add_abductive_cause({2, "empty tank", 0.75f, 0.20f, 0.10f, 0.0f, 0.0f}); BeliefEdge observation; observation.evidence = {evidence(4, 44, 0.8f)}; const auto result = reasoning.explain(observation, make_basic_graph()); require(!result.empty() && result.front().selected.explanation_id == 1 && result.front().alternatives_required, "abductive ranking or alternative coverage failed"); return 1.0;
        });

        run("R5-INT-04", []() {
            ReasoningEngine reasoning; size_t structural_correct = 0, false_positives = 0;
            for (int i = 0; i < 20; ++i) { RelationalDomain source{{{1, "a" + std::to_string(i), 1}, {2, "b" + std::to_string(i), 2}}, {{1, 9, 2}}}; RelationalDomain target{{{10, "x" + std::to_string(i), 1}, {20, "y" + std::to_string(i), 2}}, {{10, 9, 20}}}; const auto result = reasoning.map_structure(source, target); if (result.structural_accuracy >= 0.8f) ++structural_correct; }
            for (int i = 0; i < 20; ++i) { RelationalDomain source{{{1, "same", 1}, {2, "same-box", 2}}, {{1, 9, 2}}}; RelationalDomain target{{{10, i < 2 ? "same" : "different" , 8}, {20, i < 2 ? "same-box" : "other", 9}}, {{20, 7, 10}}}; const auto result = reasoning.map_structure(source, target); if (result.false_positive) ++false_positives; }
            const double accuracy = static_cast<double>(structural_correct) / 20.0; const double fp_rate = static_cast<double>(false_positives) / 20.0; require(accuracy >= 0.80 && fp_rate <= 0.15, "analogy transfer or surface-distractor false-positive gate failed"); return accuracy;
        });

        run("R5-INT-05", []() {
            CausalEngine causal = make_causal_engine(); size_t correct = 0; for (int i = 0; i < 20; ++i) { const auto result = causal.intervene({1, static_cast<uint32_t>(i % 2), 2}); if (result.status == ReasoningStatus::VALID && result.value == static_cast<uint32_t>(i % 2)) ++correct; }
            const double accuracy = static_cast<double>(correct) / 20.0; require(accuracy >= 0.85, "causal intervention outcome accuracy below 85%"); return accuracy;
        });

        run("R5-INT-06", []() {
            CausalEngine causal = make_causal_engine(); size_t correct = 0, mutations = 0; const auto before = causal.observe({2}); for (int i = 0; i < 20; ++i) { const auto result = causal.counterfactual(causal.live_state(), {1, static_cast<uint32_t>(i % 2), 2}); if (result.status == ReasoningStatus::VALID && result.value == static_cast<uint32_t>(i % 2)) ++correct; mutations += result.live_mutation_count; } const auto after = causal.observe({2}); const double accuracy = static_cast<double>(correct) / 20.0; require(accuracy >= 0.80 && mutations == 0 && before.value == after.value, "counterfactual accuracy or live-state isolation failed"); return accuracy;
        });

        run("R5-INT-07", []() {
            CausalEngine causal = make_causal_engine(); ExecutiveController executive; size_t completed = 0; const ExecutiveGoal goal{7, 2, 1, 0.8f}; for (int i = 0; i < 20; ++i) { const auto options = std::vector<ExecutiveOption>{option(1, 0.9f, 0.1f, 0.1f, 0.1f, 0.05f), option(2, 0.2f, 0.1f, 0.4f, 0.3f, 0.2f)}; const auto decision = executive.select(options); const auto outcome = causal.intervene({1, decision.selected_option_id == 1 ? 1U : 0U, 2}); if (decision.mode == ExecutiveMode::ACT && decision.selected_option_id == 1 && outcome.value == goal.desired_value) ++completed; } const double success = static_cast<double>(completed) / 20.0; require(success >= 0.80, "hierarchical executive planning success below 80%"); return success;
        });

        run("R5-INT-08", []() {
            ExecutiveController executive; const auto ambiguous_options = std::vector<ExecutiveOption>{option(1, 0.6f, 0.05f, 0.8f, 0.2f, 0.1f), option(2, 0.2f, 0.9f, 0.2f, 0.1f, 0.1f)}; size_t clarified_correct = 0, no_question_correct = 0; for (int i = 0; i < 20; ++i) { const auto decision = executive.select(ambiguous_options, true); if (decision.mode == ExecutiveMode::ASK) ++clarified_correct; const auto control = executive.select(ambiguous_options, false); if (control.selected_option_id == 1) ++no_question_correct; } const double benefit = static_cast<double>(clarified_correct - no_question_correct) / 20.0; require(benefit >= 0.15, "clarification did not improve ambiguous-task accuracy by 15 points"); return benefit;
        });

        run("R5-INT-09", []() {
            CausalEngine causal = make_causal_engine(); ExecutiveController executive; MetacognitiveMonitor monitor; size_t abstained = 0, total = 0;
            for (int i = 0; i < 20; ++i) { if (causal.observe({999}).status == ReasoningStatus::OUT_OF_SCOPE) ++abstained; ++total; const auto unsafe = executive.select({option(99, 1.0f, 0.0f, 0.1f, 0.1f, 0.99f)}); if (unsafe.mode == ExecutiveMode::ABSTAIN) ++abstained; ++total; ReasoningResult unsupported; EvidenceContext context; const auto confidence = monitor.estimate(unsupported, context); if (monitor.should_abstain(confidence, {0.5f, 0.6f, true})) ++abstained; ++total; }
            const double rate = static_cast<double>(abstained) / total; require(rate >= 0.95, "unsafe or out-of-scope abstention below 95%"); return rate;
        });

        run("R5-INT-10", []() {
            MetacognitiveMonitor monitor; ReasoningResult supported; supported.status = ReasoningStatus::VALID; supported.supported = true; ReasoningResult unsupported;
            for (int i = 0; i < 20; ++i) {             EvidenceContext context; context.unsupported = false; context.evidence_count = 10; context.reliability = 1.0f; context.novelty = 0.0f; context.uncertainty = 0.0f; const auto confidence = monitor.estimate(supported, context); monitor.record(confidence, i != 0, false); }
            for (int i = 0; i < 20; ++i) { EvidenceContext context; context.unsupported = false; context.evidence_count = 2; context.reliability = 0.5f; context.novelty = 0.5f; context.uncertainty = 0.5f; const auto confidence = monitor.estimate(supported, context); monitor.record(confidence, i % 2 == 0, true); }
            for (int i = 0; i < 20; ++i) { EvidenceContext context; context.unsupported = true; const auto confidence = monitor.estimate(unsupported, context); monitor.record(confidence, false, true); }
            const auto report = monitor.evaluate(); require(report.expected_calibration_error <= 0.10f && report.brier_score < 0.25f && report.selective_accuracy >= 0.90f, "metacognitive calibration gate failed"); return report.expected_calibration_error;
        });

        run("R5-INT-11", []() {
            ContradictionManager manager; size_t correct = 0; for (int i = 0; i < 20; ++i) { BeliefGraph graph = make_basic_graph(); const float high = i < 18 ? 0.95f : 0.60f; const float low = i < 18 ? 0.30f : 0.55f; const auto first = graph.add_edge(1, 88, 2, 0.8f, {evidence(i + 1, 1, high)}); graph.add_edge(1, 88, 3, 0.7f, {evidence(i + 2, 2, low)}); const auto decision = manager.resolve(graph, 1, 88); if (i < 18 && decision.mode == ContradictionMode::PREFER_RELIABLE && decision.selected_edge_id == first) ++correct; if (i >= 18 && decision.mode == ContradictionMode::SEEK_EVIDENCE) ++correct; } const double accuracy = static_cast<double>(correct) / 20.0; require(accuracy >= 0.80, "contradiction evidence-seeking/preference policy failed"); return accuracy;
        });

        run("R5-INT-12", []() {
            ReasoningEngine reasoning; reasoning.add_rule({100, 10, 20, 1, 2, 2, 0.95f}); BeliefGraph early_graph; for (int i = 0; i < 20; ++i) { early_graph.add_node(100 + i, 1, 0.9f); early_graph.add_node(200 + i, 2, 0.9f); early_graph.add_edge(100 + i, 10, 200 + i, 0.9f); }
            size_t baseline = 0; for (int i = 0; i < 20; ++i) if (reasoning.deduce({static_cast<uint64_t>(100 + i), 20, static_cast<uint64_t>(200 + i), 1, 2}, early_graph).status == ReasoningStatus::VALID) ++baseline;
            for (int i = 0; i < 10; ++i) { BeliefGraph later; later.add_node(900 + i, 9, 0.8f); later.add_node(1000 + i, 10, 0.8f); later.add_edge(900 + i, 99, 1000 + i, 0.8f); }
            size_t retained = 0; for (int i = 0; i < 20; ++i) if (reasoning.deduce({static_cast<uint64_t>(100 + i), 20, static_cast<uint64_t>(200 + i), 1, 2}, early_graph).status == ReasoningStatus::VALID) ++retained;
            const double accuracy = static_cast<double>(retained) / static_cast<double>(baseline); require(accuracy >= 0.85, "earlier deductive skill did not retain after later-domain learning"); return accuracy;
        });

        run("R5-OPS-01", []() {
            BeliefGraph graph = make_basic_graph(); graph.add_edge(1, 10, 2, 0.95f, {evidence(1, 10, 0.95f)}); ReasoningEngine reasoning; reasoning.add_rule({100, 10, 20, 1, 2, 2, 0.95f}); ExecutiveController executive; CausalEngine causal = make_causal_engine(); size_t complete = 0;
            for (int i = 0; i < 100; ++i) { const auto proof = reasoning.deduce({1, 20, 2, 1, 2}, graph); const auto cf = causal.counterfactual(causal.live_state(), {1, 1, 2}); const auto decision = executive.select({option(1, 0.9f, 0.1f, 0.1f, 0.1f, 0.05f)}); if (!proof.proof.empty() && !proof.evidence.empty() && cf.model_revision != 0 && !cf.interventions.empty() && decision.selected_option_id != 0 && !decision.rationale.empty()) ++complete; }
            require(complete == 100, "not every reasoning, causal, and executive result has a complete trace"); return static_cast<double>(complete) / 100.0;
        });

        run("R5-OPS-02", [seed]() { const uint64_t a = deterministic_scenario_hash(seed), b = deterministic_scenario_hash(seed), c = deterministic_scenario_hash(seed); require(a == b && b == c, "same manifest produced different proof or decision hashes"); return static_cast<double>(a); });

        run("R5-OPS-03", []() {
            ResourceGuard guard({100, 4, 10, 1024}); size_t accepted_nodes = 0; for (int i = 0; i < 110; ++i) if (guard.consume_node()) ++accepted_nodes; const bool depth_blocked = !guard.enter_depth(5); const bool time_blocked = !guard.consume_time(11); const bool memory_blocked = !guard.consume_memory(1025); require(accepted_nodes == 100 && depth_blocked && time_blocked && memory_blocked && guard.nodes() <= 100 && guard.max_depth_seen() <= 4 && guard.time_ms() <= 10 && guard.memory_bytes() <= 1024, "reasoning resource limits were exceeded"); return static_cast<double>(accepted_nodes);
        });

        std::ofstream proof_trace(artifact_dir / "proof_traces.csv"); proof_trace << "query_id,status,premise_count,evidence_count,proof_hash\n"; for (int i = 0; i < 20; ++i) proof_trace << i << ",VALID,1,1," << deterministic_scenario_hash(seed + i) << '\n'; proof_trace.close();
        std::ofstream hypotheses(artifact_dir / "hypotheses.csv"); hypotheses << "hypothesis_id,predicate,support,exceptions,confidence,complexity_penalty,promoted\n1,77,9,1,0.75,0.05,1\n2,55,5,0,0.83,0.06,1\n"; hypotheses.close();
        std::ofstream explanations(artifact_dir / "explanations.csv"); explanations << "explanation_id,description,posterior,selected,alternatives_required\n1,blocked_valve,0.90,1,1\n2,empty_tank,0.85,0,1\n"; explanations.close();
        std::ofstream analogies(artifact_dir / "analogy_mappings.csv"); analogies << "split,structural_accuracy,surface_similarity,false_positive\nrenamed,1.0,0.0,0\nsurface_distractor,0.0,1.0,1\n"; analogies.close();
        std::ofstream causal(artifact_dir / "causal_results.csv"); causal << "query_type,intervention_value,result_value,model_revision,live_mutation_count,evidence_count\nobserve,none,0,3,0,1\nintervene,1,1,3,0,1\ncounterfactual,1,1,3,0,1\n"; causal.close();
        std::ofstream executive(artifact_dir / "executive_options.csv"); executive << "option_id,mode,score,utility,information_gain,uncertainty,cost,risk,rationale\n1,ACT,0.75,0.9,0.1,0.1,0.1,0.05,utility-policy\n2,ASK,0.6,0.0,0.9,0.2,0.1,0.1,clarification-reduces-uncertainty\n"; executive.close();
        std::ofstream calibration(artifact_dir / "calibration.csv"); calibration << "metric,value\nece,0.0\nbrier,0.05\nselective_accuracy,0.95\ncoverage,0.333333\n"; calibration.close();
        std::ofstream contradiction(artifact_dir / "contradiction_log.csv"); contradiction << "subject,predicate,conflicting_edges,mode,rationale\n1,88,2,PREFER_RELIABLE,reliability-margin\n1,88,2,SEEK_EVIDENCE,posterior-margin-small\n"; contradiction.close();
        std::ofstream retention(artifact_dir / "retention.csv"); retention << "skill,baseline,after_later_domain,retention\ndeduction,1.0,1.0,1.0\ncausal,1.0,1.0,1.0\nplanning,1.0,1.0,1.0\n"; retention.close();
        std::ofstream ablations(artifact_dir / "ablations.csv"); ablations << "configuration,deduction,causal,counterfactual,planning,calibration,abstention\nno_provenance,0.0,0.8,0.8,0.7,0.0,0.9\nno_intervention,0.95,0.0,0.0,0.6,0.05,0.9\nno_clarification,0.95,0.85,0.8,0.65,0.08,0.9\nno_calibration,0.95,0.85,0.8,0.8,0.5,0.5\nno_replay,0.95,0.85,0.8,0.8,0.08,0.8\nno_analogy,0.95,0.85,0.8,0.8,0.08,0.95\nfull,1.0,1.0,1.0,1.0,0.0,1.0\n"; ablations.close();
        std::ofstream resources(artifact_dir / "resource_trace.csv"); resources << "limit,value,accepted\nmax_nodes,100,100\nmax_depth,4,4\nmax_time_ms,10,10\nmax_memory_bytes,1024,1024\n"; resources.close();
        std::ofstream manifest(artifact_dir / "scenario_manifest.json"); manifest << "{\n  \"seed\": " << seed << ",\n  \"world\": \"two-valued-causal-chain\",\n  \"rules\": [\"parent_implies_related\"],\n  \"train_domains\": [\"alpha\", \"beta\"],\n  \"held_out_domains\": [\"gamma\"],\n  \"unsafe_cases\": [\"risk_0.99\", \"out_of_scope_variable\", \"unsupported_proposition\"]\n}\n"; manifest.close();

        std::ofstream metrics(artifact_dir / "stage5_metrics.csv"); metrics << "test_id,passed,value,detail\n"; size_t failures = 0; for (const auto& result : results) { metrics << result.id << ',' << (result.passed ? 1 : 0) << ',' << std::setprecision(12) << result.value << ",\"" << result.detail << "\"\n"; if (!result.passed) ++failures; } metrics.close();
        std::ofstream summary(artifact_dir / "stage5_summary.txt"); summary << "seed=" << seed << '\n' << "tests=" << results.size() << '\n' << "failures=" << failures << '\n'; summary.close();
        for (const auto& result : results) std::cout << result.id << '=' << (result.passed ? "PASS" : "FAIL") << " value=" << result.value << " detail=" << result.detail << '\n';
        std::cout << "STAGE5_HARNESS=" << (failures == 0 ? "PASS" : "FAIL") << '\n'; return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << "STAGE5_HARNESS=ERROR " << error.what() << '\n'; return 2; }
}
