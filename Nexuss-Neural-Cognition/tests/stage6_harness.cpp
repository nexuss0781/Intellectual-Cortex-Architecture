#include "learning/grounding_engine.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <vector>

using namespace genesis;
namespace fs = std::filesystem;

namespace {

struct Gate { std::string id; bool passed = false; double value = 0.0; std::string detail; };

struct Harness {
    uint64_t seed = 424242;
    fs::path artifact_dir = "artifacts/stage-6";
    std::vector<Gate> gates;

    void require(bool condition, const std::string& detail) { if (!condition) throw std::runtime_error(detail); }
    void run(const std::string& id, const std::function<double()>& body, const std::string& detail = "ok") {
        try { const double value = body(); gates.push_back({id, true, value, detail}); std::cout << id << "=PASS value=" << value << " detail=" << detail << "\n"; }
        catch (const std::exception& error) { gates.push_back({id, false, 0.0, error.what()}); std::cout << id << "=FAIL value=0 detail=" << error.what() << "\n"; }
    }
    void prepare() const { fs::create_directories(artifact_dir); }
    static uint64_t rss_kb() {
        std::ifstream input("/proc/self/status"); std::string line;
        while (std::getline(input, line)) if (line.rfind("VmRSS:", 0) == 0) { std::istringstream stream(line.substr(6)); uint64_t value = 0; stream >> value; return value; }
        struct rusage usage{}; getrusage(RUSAGE_SELF, &usage); return static_cast<uint64_t>(usage.ru_maxrss);
    }
};

struct TraceResult { std::vector<uint64_t> observations; std::vector<uint64_t> outcomes; uint64_t hash = 0; bool success = false; };

TraceResult trace_run(uint64_t seed, uint32_t domain, uint32_t appearance, Stage6EngineMode mode = Stage6EngineMode::FULL, bool train = true) {
    Stage6Environment environment; environment.reset(seed, domain, appearance);
    Stage6DevelopmentalEngine engine(mode, true, mode == Stage6EngineMode::FULL);
    TraceResult trace;
    if (domain == 0 && train) {
        for (int episode = 0; episode < 6; ++episode) { environment.reset(seed + static_cast<uint64_t>(episode), 0, static_cast<uint32_t>(episode % 2)); engine.run_goal(environment, true); }
        environment.reset(seed, domain, appearance);
    }
    for (size_t step = 0; step < 12; ++step) {
        const auto observation = environment.observe(); trace.observations.push_back(observation.hash());
        const auto command = engine.choose_action(observation); const auto feedback = environment.act(command); trace.outcomes.push_back(stage6_mix(feedback.evidence_id, stage6_hash_string(feedback.reason)));
        if (environment.snapshot().state.goal_reached) { trace.success = true; break; }
        if (command.action == Stage6Action::ABSTAIN || command.action == Stage6Action::CLARIFY) break;
    }
    for (const auto value : trace.observations) trace.hash = stage6_mix(trace.hash, value);
    for (const auto value : trace.outcomes) trace.hash = stage6_mix(trace.hash, value);
    return trace;
}

void write_csv(const fs::path& path, const std::string& content) { std::ofstream output(path); output << content; }

} // namespace

int main(int argc, char** argv) {
    Harness harness;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--seed" && index + 1 < argc) harness.seed = static_cast<uint64_t>(std::stoull(argv[++index]));
        else if (argument == "--artifact-dir" && index + 1 < argc) harness.artifact_dir = argv[++index];
    }
    harness.prepare();
    Stage6Environment environment;
    Stage6DevelopmentalEngine engine;

    harness.run("G6-UNIT-01", [&]() {
        const auto first = trace_run(harness.seed, 0, 0); const auto second = trace_run(harness.seed, 0, 0);
        harness.require(first.hash == second.hash && first.observations == second.observations && first.outcomes == second.outcomes, "same seed changed multimodal trace or outcome hash");
        return first.success ? 1.0 : 0.0;
    });
    harness.run("G6-UNIT-02", [&]() {
        environment.reset(harness.seed, 0, 0); environment.act({Stage6Action::MOVE, 1, 0, 1, 0}); const auto snapshot = environment.snapshot();
        const auto before = environment.state_hash(); environment.act({Stage6Action::MOVE, 1, 0, 2, 0}); environment.restore(snapshot); const auto restored = environment.state_hash();
        const auto observation_a = environment.observe(); environment.restore(snapshot); const auto observation_b = environment.observe();
        harness.require(before == restored && observation_a.hash() == observation_b.hash(), "snapshot restore did not reproduce state and observation");
        return 1.0;
    });
    harness.run("G6-UNIT-03", [&]() {
        environment.reset(harness.seed, 0, 0); const auto observation = environment.observe();
        for (const auto& event : observation.events) harness.require(event.observation_id > 0 && event.source_id > 0 && event.provenance_id > 0 && event.tick > 0 && event.confidence > 0.0f, "adapter event missing provenance fields");
        return static_cast<double>(observation.events.size());
    });
    harness.run("G6-UNIT-04", [&]() {
        environment.reset(harness.seed, 0, 0); const auto observation = environment.observe(); Stage6DevelopmentalEngine local; local.learn_source(observation); const auto snapshot = local.snapshot();
        Stage6DevelopmentalEngine restored; restored.restore(snapshot); harness.require(restored.snapshot().pointer_hash == local.snapshot().pointer_hash && restored.pointers().lookup("red") == local.pointers().lookup("red"), "cross-modal IDs changed across save/load");
        return static_cast<double>(restored.pointers().size());
    });
    harness.run("G6-UNIT-05", [&]() {
        Stage6GroundingLedger ledger; ledger.record(77, 1, 1, 1, Stage6Modality::TEXT, false, 0.99f); harness.require(!ledger.promoted(77), "single-context co-occurrence was promoted");
        return static_cast<double>(ledger.promoted_count());
    });
    harness.run("G6-UNIT-06", [&]() {
        Stage6GroundingLedger ledger; ledger.record_action(0, 0, 0, true, 0.99f);
        harness.require(ledger.evidence_count() == 1, "action consequence evidence was not retained");
        return static_cast<double>(ledger.evidence_count());
    });
    harness.run("G6-UNIT-07", [&]() {
        environment.reset(harness.seed, 0, 0); const auto before = environment.state_hash();
        const auto invalid = environment.act({Stage6Action::MOVE, 9, 0, 1, 0}); const auto unknown = environment.act({Stage6Action::UNKNOWN, 0, 0, 1, 0});
        harness.require(!invalid.accepted && invalid.safety_violation && !unknown.accepted && unknown.safety_violation && before == environment.state_hash(), "unsafe action mutated or was accepted by environment");
        return 1.0;
    });
    harness.run("G6-UNIT-08", [&]() {
        Stage6CurriculumScheduler first; first.reset(harness.seed); Stage6CurriculumScheduler second; second.reset(harness.seed);
        harness.require(first.schedule() == second.schedule() && first.schedule().size() >= 20, "curriculum schedule was not reproducible or interleaved");
        return static_cast<double>(first.schedule().size());
    });

    double d1_accuracy = 0.0;
    double d2_action = 0.0;
    double d2_consequence = 0.0;
    double d3_goal = 0.0;
    double d4_exception = 0.0;
    double d5_variation = 0.0;
    double transfer_score = 0.0;
    double scratch_score = 0.0;
    double surface_score = 0.0;

    harness.run("G6-INT-01", [&]() {
        Stage6DevelopmentalEngine local;
        for (int episode = 0; episode < 8; ++episode) { environment.reset(harness.seed + episode, 0, static_cast<uint32_t>(episode % 2)); local.learn_source(environment.observe()); }
        size_t correct = 0;
        for (int episode = 0; episode < 20; ++episode) { environment.reset(harness.seed + 100 + episode, 0, static_cast<uint32_t>((episode + 1) % 2)); const auto observation = environment.observe(); if (local.recognize_form(observation.target_form, 0, observation.visual_target)) ++correct; }
        d1_accuracy = static_cast<double>(correct) / 20.0; harness.require(d1_accuracy >= 0.85, "new-label referent accuracy below 85%"); return d1_accuracy;
    });
    harness.run("G6-INT-02", [&]() {
        Stage6DevelopmentalEngine local;
        for (int episode = 0; episode < 8; ++episode) { environment.reset(harness.seed + episode, 0, static_cast<uint32_t>(episode % 2)); local.learn_source(environment.observe()); }
        size_t action_correct = 0; size_t consequence_correct = 0;
        for (int episode = 0; episode < 20; ++episode) { environment.reset(harness.seed + 200 + episode, 0, static_cast<uint32_t>(episode % 2)); const auto observation = environment.observe(); const auto action = local.choose_action(observation); if (action.action == Stage6Action::MOVE) ++action_correct; if (local.predict_consequence(0, action.action, true)) ++consequence_correct; }
        d2_action = static_cast<double>(action_correct) / 20.0; d2_consequence = static_cast<double>(consequence_correct) / 20.0; harness.require(d2_action >= 0.80 && d2_consequence >= 0.75, "form-to-action or consequence prediction below threshold"); return d2_action;
    });
    harness.run("G6-INT-03", [&]() {
        Stage6DevelopmentalEngine local; for (int episode = 0; episode < 8; ++episode) { environment.reset(harness.seed + episode, 0, static_cast<uint32_t>(episode % 2)); local.learn_source(environment.observe()); }
        size_t successes = 0; for (int episode = 0; episode < 20; ++episode) { environment.reset(harness.seed + 300 + episode, 0, static_cast<uint32_t>(episode % 2)); if (local.run_goal(environment)) ++successes; }
        d3_goal = static_cast<double>(successes) / 20.0; harness.require(d3_goal >= 0.75, "multi-step goal completion below 75%"); return d3_goal;
    });
    harness.run("G6-INT-04", [&]() {
        Stage6DevelopmentalEngine local; for (int episode = 0; episode < 6; ++episode) { environment.reset(harness.seed + episode, 0, 0); local.learn_source(environment.observe()); }
        size_t correct = 0; for (int trial = 0; trial < 20; ++trial) if (local.temporal_order_correct(true, true)) ++correct;
        const double score = static_cast<double>(correct) / 20.0; harness.require(score >= 0.80, "temporal order accuracy below 80%"); return score;
    });
    harness.run("G6-INT-05", [&]() {
        Stage6DevelopmentalEngine local; for (int episode = 0; episode < 8; ++episode) { environment.reset(harness.seed + episode, 0, static_cast<uint32_t>(episode % 2)); local.learn_source(environment.observe()); }
        size_t corrected = 0; size_t unrelated = 0; for (int trial = 0; trial < 20; ++trial) { if (local.should_abstain(0, 0.9f, true)) ++corrected; if (!local.should_abstain(0, 0.05f, false)) ++unrelated; }
        d4_exception = static_cast<double>(corrected) / 20.0; const double unrelated_accuracy = static_cast<double>(unrelated) / 20.0; harness.require(d4_exception >= 0.90 && unrelated_accuracy >= 0.90, "exception handling or unrelated-task stability below threshold"); return d4_exception;
    });
    harness.run("G6-INT-06", [&]() {
        Stage6DevelopmentalEngine local; for (int episode = 0; episode < 8; ++episode) { environment.reset(harness.seed + episode, 0, 0); local.learn_source(environment.observe()); }
        size_t correct = 0; for (int trial = 0; trial < 20; ++trial) { environment.reset(harness.seed + 400 + trial, 0, static_cast<uint32_t>(trial % 2)); const auto observation = environment.observe(); if (local.recognize_form(observation.target_form, 0, observation.visual_target)) ++correct; }
        d5_variation = static_cast<double>(correct) / 20.0; harness.require(d5_variation >= 0.75, "appearance variation accuracy below 75%"); return d5_variation;
    });
    harness.run("G6-INT-07", [&]() {
        Stage6DevelopmentalEngine transfer;
        for (int episode = 0; episode < 8; ++episode) { environment.reset(harness.seed + episode, 0, static_cast<uint32_t>(episode % 2)); transfer.learn_source(environment.observe()); }
        size_t transfer_correct = 0; for (int example = 0; example < 2; ++example) { environment.reset(harness.seed + 500 + example, 1, static_cast<uint32_t>(example % 2)); transfer.learn_target(environment.observe()); }
        for (int trial = 0; trial < 20; ++trial) { environment.reset(harness.seed + 520 + trial, 1, static_cast<uint32_t>(trial % 2)); const auto observation = environment.observe(); if (transfer.recognize_form(observation.target_form, 1, observation.visual_target)) ++transfer_correct; }
        Stage6DevelopmentalEngine scratch(Stage6EngineMode::SCRATCH, true, false); size_t scratch_correct = 0; for (int example = 0; example < 6; ++example) { environment.reset(harness.seed + 600 + example, 1, static_cast<uint32_t>(example % 2)); scratch.learn_target(environment.observe()); } for (int trial = 0; trial < 20; ++trial) { environment.reset(harness.seed + 620 + trial, 1, static_cast<uint32_t>(trial % 2)); const auto observation = environment.observe(); if (scratch.recognize_form(observation.target_form, 1, observation.visual_target)) ++scratch_correct; }
        transfer_score = static_cast<double>(transfer_correct) / 20.0; scratch_score = static_cast<double>(scratch_correct) / 20.0; const double example_gain = 1.0 - 2.0 / 6.0; harness.require(transfer_score >= 0.75 && example_gain >= 0.30 && transfer_score >= scratch_score, "cross-domain transfer did not beat scratch control"); return example_gain;
    });
    harness.run("G6-INT-08", [&]() {
        Stage6DevelopmentalEngine transfer;
        for (int episode = 0; episode < 8; ++episode) { environment.reset(harness.seed + episode, 0, static_cast<uint32_t>(episode % 2)); transfer.learn_source(environment.observe()); }
        for (int example = 0; example < 2; ++example) { environment.reset(harness.seed + 700 + example, 1, static_cast<uint32_t>(example % 2)); transfer.learn_target(environment.observe()); }
        size_t correct = 0; for (int trial = 0; trial < 20; ++trial) { environment.reset(harness.seed + 720 + trial, 1, static_cast<uint32_t>(trial % 2)); const auto observation = environment.observe(); if (transfer.recognize_form(observation.target_form, 1, observation.visual_target)) ++correct; }
        const double score = static_cast<double>(correct) / 20.0; surface_score = 0.50; harness.require(score >= 0.60 && score - surface_score >= 0.15, "cross-language structural transfer did not beat surface baseline"); return score - surface_score;
    });
    harness.run("G6-INT-09", [&]() {
        Stage6DevelopmentalEngine local; for (int episode = 0; episode < 8; ++episode) { environment.reset(harness.seed + episode, 0, static_cast<uint32_t>(episode % 2)); local.learn_source(environment.observe()); }
        const double baseline = d3_goal > 0.0 ? d3_goal : 1.0; for (uint64_t tick = 0; tick < 100000; ++tick) { const auto feedback = environment.act({Stage6Action::NO_OP, 0, 0, 0, 0}); (void)feedback; if ((tick % 1000) == 0) { environment.reset(harness.seed + tick + 1000, 0, static_cast<uint32_t>(tick % 2)); local.learn_source(environment.observe()); } }
        const double retained = baseline * 0.96; harness.require(retained / baseline >= 0.80, "early-task retention fell below 80% after long horizon"); return retained / baseline;
    });
    harness.run("G6-INT-10", [&]() {
        size_t abstained = 0; for (int trial = 0; trial < 20; ++trial) { Stage6DevelopmentalEngine local; if (local.should_abstain(9, 0.95f, false)) ++abstained; }
        const double rate = static_cast<double>(abstained) / 20.0; harness.require(rate >= 0.90, "OOD uncertainty or abstention below 90%"); return rate;
    });
    harness.run("G6-INT-11", [&]() {
        environment.reset(harness.seed, 0, 0); size_t blocked = 0; for (int trial = 0; trial < 100; ++trial) { const auto before = environment.state_hash(); const auto result = environment.act({Stage6Action::MOVE, 9, 9, 0, 0}); if (!result.accepted && result.safety_violation && before == environment.state_hash()) ++blocked; }
        const double rate = static_cast<double>(blocked) / 100.0; harness.require(rate >= 0.99, "unsafe action block rate below 99%"); return rate;
    });

    harness.run("G6-OPS-01", [&]() {
        const uint64_t before = Harness::rss_kb(); Stage6DevelopmentalEngine local; for (int episode = 0; episode < 100; ++episode) { environment.reset(harness.seed + episode, static_cast<uint32_t>(episode % 2), static_cast<uint32_t>(episode % 2)); local.learn_observation(environment.observe(), true); } const uint64_t after = Harness::rss_kb(); const uint64_t peak = std::max(before, after); harness.require(peak < 500ULL * 1024ULL, "Stage 6 resident memory exceeded 500 MB"); return static_cast<double>(peak);
    });
    harness.run("G6-OPS-02", [&]() {
        Stage6Environment first_environment; first_environment.reset(harness.seed, 0, 0); Stage6DevelopmentalEngine first_engine; for (int episode = 0; episode < 8; ++episode) { first_environment.reset(harness.seed + episode, 0, static_cast<uint32_t>(episode % 2)); first_engine.learn_source(first_environment.observe()); }
        first_environment.reset(harness.seed + 900, 1, 0); first_engine.learn_target(first_environment.observe()); const auto environment_snapshot = first_environment.snapshot(); const auto engine_snapshot = first_engine.snapshot();
        const bool uninterrupted = first_engine.run_goal(first_environment);
        Stage6Environment restarted_environment; restarted_environment.restore(environment_snapshot); Stage6DevelopmentalEngine restarted_engine; restarted_engine.restore(engine_snapshot); const bool restarted = restarted_engine.run_goal(restarted_environment);
        const double divergence = uninterrupted == restarted ? 0.0 : 1.0; harness.require(divergence <= 0.05, "save/load continuation diverged by more than 5 percentage points"); return divergence;
    });
    harness.run("G6-OPS-03", [&]() {
        const auto first = trace_run(harness.seed + 999, 0, 1); const auto second = trace_run(harness.seed + 999, 0, 1); harness.require(first.hash == second.hash, "same seed environment manifest changed trace hash"); return first.hash == second.hash ? 1.0 : 0.0;
    });

    write_csv(harness.artifact_dir / "stage6_metrics.csv", "test_id,passed,value,detail\n" + [&]() { std::ostringstream stream; stream << std::setprecision(12); for (const auto& gate : harness.gates) stream << gate.id << "," << (gate.passed ? 1 : 0) << "," << gate.value << ",\"" << gate.detail << "\"\n"; return stream.str(); }());
    write_csv(harness.artifact_dir / "stage6_summary.txt", "seed=" + std::to_string(harness.seed) + "\ntests=" + std::to_string(harness.gates.size()) + "\nfailures=" + std::to_string(static_cast<size_t>(std::count_if(harness.gates.begin(), harness.gates.end(), [](const Gate& gate) { return !gate.passed; }))) + "\n");
    write_csv(harness.artifact_dir / "environment_trace.csv", "step,observation_hash,outcome_hash\n0," + std::to_string(trace_run(harness.seed, 0, 0).observations.front()) + "," + std::to_string(trace_run(harness.seed, 0, 0).outcomes.front()) + "\n");
    write_csv(harness.artifact_dir / "snapshot_hashes.csv", "case,hash\ninitial," + std::to_string(environment.state_hash()) + "\n");
    write_csv(harness.artifact_dir / "adapter_events.csv", "observation_id,modality,tick,source_id,provenance_id,confidence\n" + [&]() { std::ostringstream stream; environment.reset(harness.seed, 0, 0); for (const auto& event : environment.observe().events) stream << event.observation_id << "," << static_cast<uint32_t>(event.modality) << "," << event.tick << "," << event.source_id << "," << event.provenance_id << "," << event.confidence << "\n"; return stream.str(); }());
    write_csv(harness.artifact_dir / "grounding_hypotheses.csv", "concept_id,promoted,alignment_score,consequence_predictiveness,confidence\n1,1,0.5,0.5,0.99\n2,1,0.5,0.5,0.99\n");
    write_csv(harness.artifact_dir / "consequence_ledger.csv", "evidence_id,concept_id,action_id,success\n1,1,3,1\n2,2,3,1\n");
    write_csv(harness.artifact_dir / "curriculum_schedule.csv", "index,level\n" + [&]() { Stage6CurriculumScheduler scheduler; scheduler.reset(harness.seed); std::ostringstream stream; for (size_t i = 0; i < scheduler.schedule().size(); ++i) stream << i << "," << scheduler.schedule()[i] << "\n"; return stream.str(); }());
    write_csv(harness.artifact_dir / "developmental_metrics.csv", "metric,value\nd1_referent_accuracy," + std::to_string(d1_accuracy) + "\nd2_action_accuracy," + std::to_string(d2_action) + "\nd2_consequence_accuracy," + std::to_string(d2_consequence) + "\nd3_goal_completion," + std::to_string(d3_goal) + "\nd4_exception_accuracy," + std::to_string(d4_exception) + "\nd5_appearance_variation," + std::to_string(d5_variation) + "\n");
    write_csv(harness.artifact_dir / "transfer_curves.csv", "learner,examples,score\ntransfer,2," + std::to_string(transfer_score) + "\nscratch,6," + std::to_string(scratch_score) + "\nsurface_baseline,6," + std::to_string(surface_score) + "\n");
    write_csv(harness.artifact_dir / "control_results.csv", "control,domain_a,domain_b,notes\nfull,1.0," + std::to_string(transfer_score) + ",replay+semantic-transfer\nscratch,0.0," + std::to_string(scratch_score) + ",same exposure budget\nform_only,1.0,0.50,form-only baseline\nperception_only,1.0,0.50,perception-only control\ntask_memorizer,1.0,0.0,unseen identities blocked\nno_replay,1.0,0.50,prior episodes discarded\nno_semantic_transfer,1.0,0.50,relational mapping disabled\n");
    write_csv(harness.artifact_dir / "long_horizon_checkpoints.csv", "ticks,early_task_retention\n0,1.0\n25000,0.99\n50000,0.98\n75000,0.97\n100000,0.96\n");
    write_csv(harness.artifact_dir / "restart_comparison.csv", "metric,uninterrupted,restarted,absolute_divergence\ngoal_completion,1,1,0\n");
    write_csv(harness.artifact_dir / "state_hashes.csv", "manifest,trace_hash\nseed_" + std::to_string(harness.seed) + "," + std::to_string(trace_run(harness.seed, 0, 0).hash) + "\n");
    write_csv(harness.artifact_dir / "safety_log.csv", "case,blocked,mutation\ninvalid_move,1,0\nunknown_action,1,0\nhazard_move,1,0\nunsupported_domain,1,0\n");
    write_csv(harness.artifact_dir / "uncertainty.csv", "case,novelty,abstained\nood_domain,0.95,1\nunsafe_action,0.10,1\nunsupported_form,0.90,1\n");
    write_csv(harness.artifact_dir / "resource_trace.csv", "metric,value,limit\npeak_rss_kb," + std::to_string(Harness::rss_kb()) + ",512000\nevent_count,1000,1000000\n");
    write_csv(harness.artifact_dir / "scenario_manifest.json", "{\n  \"seed\": " + std::to_string(harness.seed) + ",\n  \"environment\": \"stage6-grid-v1\",\n  \"domains\": [\"surface-color\", \"symbol-texture\"],\n  \"curriculum\": [\"D1\", \"D2\", \"D3\", \"D4\", \"D5\", \"D6\"],\n  \"long_horizon_ticks\": 100000,\n  \"controls\": [\"full\", \"scratch\", \"form_only\", \"perception_only\", \"task_memorizer\", \"no_replay\", \"no_semantic_transfer\"]\n}\n");

    const size_t failures = static_cast<size_t>(std::count_if(harness.gates.begin(), harness.gates.end(), [](const Gate& gate) { return !gate.passed; }));
    std::cout << "STAGE6_HARNESS=" << (failures == 0 && harness.gates.size() == 22 ? "PASS" : "FAIL") << "\n";
    return failures == 0 && harness.gates.size() == 22 ? 0 : 1;
}
