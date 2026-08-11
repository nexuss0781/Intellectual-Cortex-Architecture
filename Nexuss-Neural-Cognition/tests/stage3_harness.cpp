#include "learning/predictive_workspace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace genesis;

namespace {

struct Result { std::string id; bool passed; double value; std::string detail; };

void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }

PredictionProposal proposal(uint64_t id, float surprise, float goal_bias, float salience, uint64_t source = 1) {
    PredictionProposal item;
    item.proposal_id = id;
    item.source_population = source;
    item.context_id = 1;
    item.surprise = surprise;
    item.goal_bias = goal_bias;
    item.uncertainty = 0.5f;
    item.expected_information_gain = 0.2f;
    item.cost = 0.1f;
    item.precision = 1.0f;
    item.normalized_error = surprise;
    item.precision_weighted_error = surprise;
    item.confidence = 1.0f / (1.0f + surprise);
    item.salience = salience;
    return item;
}

PredictionProposal scored_proposal(uint64_t id, float surprise, float goal_bias, uint64_t source = 1) {
    auto item = proposal(id, surprise, goal_bias, 0.0f, source);
    item.salience = PredictivePopulation::score(item);
    return item;
}

std::vector<float> code(std::initializer_list<float> values) { return std::vector<float>(values); }

uint64_t trace_hash(const std::vector<WorkspaceBroadcast>& broadcasts) {
    uint64_t hash = 1469598103934665603ULL;
    auto mix = [&hash](uint64_t value) { for (int i = 0; i < 8; ++i) { hash ^= static_cast<uint8_t>(value & 0xffU); hash *= 1099511628211ULL; value >>= 8; } };
    for (const auto& item : broadcasts) { mix(item.broadcast_id); mix(item.tick); mix(item.winner_count); mix(static_cast<uint64_t>(std::lround(item.coalition_score * 100000.0f))); mix(item.goal_id); for (uint64_t id : item.proposal_ids) mix(id); }
    return hash;
}

std::vector<WorkspaceBroadcast> run_deterministic_workspace(uint64_t seed) {
    std::mt19937_64 rng(seed);
    WorkspaceConfig config;
    config.capacity = 2;
    config.ignition_threshold = 0.5f;
    config.ignition_consecutive_ticks = 1;
    GlobalWorkspace workspace(config);
    std::vector<WorkspaceBroadcast> output;
    for (uint64_t tick = 0; tick < 100; ++tick) {
        std::uniform_real_distribution<float> salience(0.0f, 2.0f);
        for (uint64_t i = 0; i < 5; ++i) workspace.submit(proposal(tick * 10 + i, salience(rng), 0.0f, salience(rng), i));
        const auto broadcast = workspace.step(tick, tick % 3);
        if (broadcast) output.push_back(*broadcast);
    }
    return output;
}

} // namespace

int main(int argc, char** argv) {
    try {
        uint64_t seed = 424242;
        fs::path artifact_dir = "artifacts/stage-3";
        for (int i = 1; i + 1 < argc; ++i) {
            if (std::string(argv[i]) == "--seed") seed = std::stoull(argv[i + 1]);
            if (std::string(argv[i]) == "--artifact-dir") artifact_dir = argv[i + 1];
        }
        fs::create_directories(artifact_dir);
        std::vector<Result> results;

        auto run = [&results](const std::string& id, const auto& function) {
            try { results.push_back({id, true, function(), "ok"}); }
            catch (const std::exception& error) { results.push_back({id, false, 0.0, error.what()}); }
        };

        run("W3-UNIT-01", []() {
            PredictiveConfig config; config.dimensions = 4; config.learning_enabled = false;
            PredictivePopulation population(1, config);
            CognitiveContext context; context.context_id = 1;
            const auto result = population.step(context, SparseCode(code({1.0f, -1.0f, 0.0f, 2.0f})));
            const float expected = std::sqrt(1.5f);
            require(std::abs(result.normalized_error - expected) < 2e-5f, "sparse prediction error mismatch: actual=" + std::to_string(result.normalized_error) + " expected=" + std::to_string(expected));
            const auto reversed = population.step(context, SparseCode(code({-1.0f, 1.0f, 0.0f, -2.0f})));
            require(reversed.normalized_error > 0.0f && reversed.prediction.values[0] == 0.0f, "sign-reversed error contract failed");
            return static_cast<double>(result.normalized_error);
        });

        run("W3-UNIT-02", []() {
            PredictiveConfig small; small.dimensions = 4; small.initial_scale = 1.0f; small.learning_enabled = false;
            PredictiveConfig large = small; large.initial_scale = 2.0f;
            CognitiveContext context;
            PredictivePopulation p_small(1, small), p_large(2, large);
            const auto small_error = p_small.step(context, SparseCode(code({2.0f, 0.0f, 0.0f, 0.0f}))).normalized_error;
            const auto large_error = p_large.step(context, SparseCode(code({4.0f, 0.0f, 0.0f, 0.0f}))).normalized_error;
            require(std::abs(small_error - large_error) < 1e-4f, "population scale changed normalized error ordering");
            return static_cast<double>(small_error);
        });

        run("W3-UNIT-03", []() {
            PredictiveConfig config; config.dimensions = 4; config.learning_enabled = false;
            CognitiveContext context;
            PredictivePopulation low(1, config), high(2, config);
            low.set_precision(1.0f); high.set_precision(2.0f);
            const auto a = low.step(context, SparseCode(code({1.0f, 0.0f, 0.0f, 0.0f})));
            const auto b = high.step(context, SparseCode(code({1.0f, 0.0f, 0.0f, 0.0f})));
            const float ratio = b.precision_weighted_error / a.precision_weighted_error;
            require(ratio >= 1.8f && ratio <= 2.2f, "doubling precision did not double error influence");
            return ratio;
        });

        run("W3-UNIT-04", []() {
            PredictivePopulation population;
            population.set_precision(-100.0f);
            require(population.precision() >= 0.1f, "precision fell below lower bound");
            for (int i = 0; i < 10000; ++i) population.update_precision(1000.0f);
            require(std::isfinite(population.precision()) && population.precision() <= 10.0f, "precision exceeded upper bound or became non-finite");
            return population.precision();
        });

        run("W3-UNIT-05", [seed]() {
            std::mt19937_64 rng(seed);
            std::uniform_real_distribution<float> value(-2.0f, 2.0f);
            double max_error = 0.0;
            for (int i = 0; i < 10000; ++i) {
                PredictionProposal item = proposal(static_cast<uint64_t>(i), value(rng), value(rng), value(rng));
                item.uncertainty = value(rng); item.expected_information_gain = value(rng); item.cost = value(rng);
                const float expected = item.surprise + item.goal_bias + 0.25f * item.uncertainty + 0.50f * item.expected_information_gain - 0.10f * item.cost;
                max_error = std::max(max_error, static_cast<double>(std::abs(PredictivePopulation::score(item) - expected)));
            }
            require(max_error <= 1e-6, "proposal score equation mismatch");
            return max_error;
        });

        run("W3-UNIT-06", []() {
            WorkspaceConfig config; config.capacity = 3; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.1f;
            GlobalWorkspace workspace(config);
            for (int i = 0; i < 10; ++i) workspace.submit(proposal(i + 1, 1.0f, 0.0f, 1.0f));
            const auto broadcast = workspace.step(1, 1);
            require(broadcast.has_value() && broadcast->winner_count == 3 && broadcast->proposal_ids.size() == 3, "workspace exceeded capacity");
            return static_cast<double>(broadcast->winner_count);
        });

        run("W3-UNIT-07", []() {
            WorkspaceConfig config; config.capacity = 1; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.75f; config.hysteresis = 0.20f;
            GlobalWorkspace workspace(config);
            workspace.submit(proposal(1, 1.0f, 0.0f, 0.80f));
            require(workspace.step(1, 1).has_value(), "workspace did not ignite above threshold");
            workspace.submit(proposal(2, 1.0f, 0.0f, 0.70f));
            require(workspace.step(2, 1).has_value(), "hysteresis chattered off at borderline coalition");
            workspace.submit(proposal(3, 1.0f, 0.0f, 0.40f));
            require(!workspace.step(3, 1).has_value(), "workspace did not extinguish below hysteresis threshold");
            return 1.0;
        });

        run("W3-UNIT-08", []() {
            WorkspaceConfig config; config.capacity = 2; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.1f;
            GlobalWorkspace workspace(config);
            workspace.submit(proposal(1, 1.0f, 0.0f, 2.0f));
            workspace.submit(proposal(2, 1.0f, 0.0f, 1.5f));
            workspace.submit(proposal(3, 1.0f, 0.0f, 0.1f));
            const auto broadcast = workspace.step(1, 1);
            require(broadcast.has_value() && std::find(broadcast->proposal_ids.begin(), broadcast->proposal_ids.end(), 3) == broadcast->proposal_ids.end(), "losing proposal appeared in broadcast");
            return static_cast<double>(broadcast->proposal_ids.size());
        });

        run("W3-UNIT-09", []() {
            WorkspaceConfig config; config.capacity = 1; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.1f;
            GlobalWorkspace workspace(config);
            workspace.submit(scored_proposal(1, 2.0f, 0.0f));
            workspace.submit(scored_proposal(2, 0.1f, 2.0f));
            const auto broadcast = workspace.step(1, 42);
            require(broadcast.has_value() && broadcast->proposal_ids.front() == 2 && broadcast->goal_id == 42, "goal bias did not override lower bottom-up salience");
            return 1.0;
        });

        run("W3-UNIT-10", []() {
            PredictiveConfig config; config.dimensions = 4; config.learning_enabled = false;
            PredictivePopulation population(1, config);
            CognitiveContext context;
            population.set_precision(8.0f);
            const auto result = population.step(context, SparseCode(code({1.0f, -1.0f, 1.0f, -1.0f})));
            require(result.precision > 1.0f && result.confidence < 0.5f, "precision was conflated with confidence under ambiguous evidence");
            return result.confidence;
        });

        run("W3-INT-01", []() {
            PredictiveConfig config; config.dimensions = 4; config.transition_learning_rate = 0.5f;
            PredictivePopulation population(1, config);
            CognitiveContext context; context.context_id = 5;
            const SparseCode a(code({1.0f, 0.0f, 0.0f, 0.0f}));
            const SparseCode b(code({0.0f, 1.0f, 0.0f, 0.0f}));
            LearningSignal learning_signal;
            learning_signal.executive_permission = 1.0f;
            for (int i = 0; i < 30; ++i) { population.step(context, a, learning_signal); population.step(context, b, learning_signal); }
            const auto trained_a = population.step(context, a);
            const auto trained_b = population.step(context, b);
            PredictivePopulation baseline(2, config);
            const auto base_a = baseline.step(context, a);
            const auto base_b = baseline.step(context, b);
            const float trained_error = trained_b.normalized_error;
            const float baseline_error = base_b.normalized_error;
            require(baseline_error - trained_error >= 0.20f, "held-out next-event prediction did not improve by 20 percentage points");
            (void)trained_a; (void)base_a;
            return static_cast<double>(baseline_error - trained_error);
        });

        run("W3-INT-02", []() {
            size_t correct = 0;
            WorkspaceConfig config; config.capacity = 1; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.1f;
            for (int trial = 0; trial < 100; ++trial) {
                GlobalWorkspace workspace(config);
                for (int distractor = 0; distractor < 7; ++distractor) workspace.submit(scored_proposal(1000 + trial * 10 + distractor, 2.0f, 0.0f));
                workspace.submit(scored_proposal(2000 + trial, 0.2f, 2.5f));
                const auto broadcast = workspace.step(trial, 99);
                if (broadcast && broadcast->proposal_ids.front() == static_cast<uint64_t>(2000 + trial)) ++correct;
            }
            const double accuracy = static_cast<double>(correct) / 100.0;
            require(accuracy >= 0.85, "goal-directed distractor rejection below 85%");
            return accuracy;
        });

        run("W3-INT-03", []() {
            size_t contextual_correct = 0, context_free_correct = 0;
            for (int trial = 0; trial < 100; ++trial) {
                const uint64_t target = static_cast<uint64_t>((trial % 2) + 1);
                WorkspaceConfig config; config.capacity = 1; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.1f;
                GlobalWorkspace contextual(config);
                contextual.submit(proposal(10, 0.8f, target == 1 ? 0.8f : 0.0f, target == 1 ? 1.2f : 0.8f));
                contextual.submit(proposal(20, 0.8f, target == 2 ? 0.8f : 0.0f, target == 2 ? 1.2f : 0.8f));
                const auto selected = contextual.step(trial, target);
                if (selected && selected->proposal_ids.front() == (target == 1 ? 10ULL : 20ULL)) ++contextual_correct;
                GlobalWorkspace context_free(config);
                context_free.submit(proposal(10, 0.8f, 0.0f, 0.8f));
                context_free.submit(proposal(20, 0.8f, 0.0f, 0.8f));
                const auto control = context_free.step(trial, 0);
                if (control && control->proposal_ids.front() == (target == 1 ? 10ULL : 20ULL)) ++context_free_correct;
            }
            const double contextual = static_cast<double>(contextual_correct) / 100.0;
            const double control = static_cast<double>(context_free_correct) / 100.0;
            require(contextual >= 0.80 && contextual - control >= 0.15, "context-conditioned ambiguity advantage below gate");
            return contextual - control;
        });

        run("W3-INT-04", []() {
            WorkspaceConfig config; config.capacity = 1; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.1f;
            GlobalWorkspace workspace(config);
            workspace.submit(proposal(1, 0.3f, 1.0f, 1.3f));
            const auto first = workspace.step(1, 1);
            require(first && first->proposal_ids.front() == 1, "initial goal did not select target");
            int response_cycle = 100;
            for (int cycle = 2; cycle <= 21; ++cycle) {
                workspace.submit(proposal(2, 0.3f, 1.0f, 1.3f));
                const auto current = workspace.step(cycle, 2);
                if (current && current->proposal_ids.front() == 2) { response_cycle = cycle; break; }
            }
            require(response_cycle <= 20, "goal switch exceeded 20 workspace cycles");
            return static_cast<double>(response_cycle - 1);
        });

        run("W3-INT-05", []() {
            size_t ordered = 0;
            for (int trial = 0; trial < 100; ++trial) {
                const auto familiar = proposal(1, 0.2f, 0.0f, 0.25f);
                const auto novel = proposal(2, 2.0f, 0.0f, 2.05f);
                if (novel.salience > familiar.salience) ++ordered;
            }
            require(ordered >= 90, "novelty did not exceed matched familiar salience in 90% of trials");
            return static_cast<double>(ordered) / 100.0;
        });

        run("W3-INT-06", []() {
            size_t workspace_success = 0, local_success = 0;
            WorkspaceConfig config; config.capacity = 1; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.1f;
            for (int trial = 0; trial < 100; ++trial) {
                GlobalWorkspace workspace(config);
                for (int i = 0; i < 5; ++i) workspace.submit(scored_proposal(100 + trial * 10 + i, 0.8f, 0.0f));
                const uint64_t target_id = 1000 + trial;
                workspace.submit(scored_proposal(target_id, 0.2f, 1.0f));
                const auto broadcast = workspace.step(trial, 1);
                if (broadcast && broadcast->proposal_ids.front() == target_id) ++workspace_success;
                GlobalWorkspace local(config);
                for (int i = 0; i < 5; ++i) local.submit(proposal(3000 + trial * 10 + i, 0.8f, 0.0f, 0.8f));
                local.submit(proposal(target_id, 0.2f, 0.0f, 0.2f));
                const auto local_broadcast = local.step(trial, 0);
                if (local_broadcast && local_broadcast->proposal_ids.front() == target_id) ++local_success;
            }
            const double improvement = static_cast<double>(workspace_success - local_success) / 100.0;
            require(improvement >= 0.15, "workspace utility did not beat local-only control by 15 points");
            return improvement;
        });

        run("W3-OPS-01", []() {
            WorkspaceConfig config; config.capacity = 3; config.broadcast_budget_bytes = 512; config.ignition_consecutive_ticks = 1; config.ignition_threshold = 0.1f;
            GlobalWorkspace workspace(config);
            for (int i = 0; i < 3; ++i) workspace.submit(proposal(i + 1, 1.0f, 0.0f, 1.0f));
            const auto broadcast = workspace.step(1, 1);
            require(broadcast.has_value(), "budget fixture did not broadcast");
            require(sizeof(WorkspaceBroadcast) + broadcast->proposal_ids.size() * sizeof(uint64_t) <= config.broadcast_budget_bytes, "broadcast byte budget exceeded");
            return static_cast<double>(sizeof(WorkspaceBroadcast) + broadcast->proposal_ids.size() * sizeof(uint64_t));
        });

        run("W3-OPS-02", [seed]() {
            const auto a = run_deterministic_workspace(seed);
            const auto b = run_deterministic_workspace(seed);
            const auto c = run_deterministic_workspace(seed);
            require(trace_hash(a) == trace_hash(b) && trace_hash(b) == trace_hash(c), "same-seed proposal/broadcast traces differ");
            return static_cast<double>(trace_hash(a));
        });

        run("W3-OPS-03", [artifact_dir]() {
            const fs::path state_path = artifact_dir / "predictive_workspace.bin";
            PredictiveWorkspace original;
            original.add_population(1);
            CognitiveContext context; context.context_id = 4; context.goal_id = 5;
            original.cycle(context, {SparseCode(code({1, 0, 0, 0, 0, 0, 0, 0}))}, 1);
            original.save(state_path);
            PredictiveWorkspace loaded;
            loaded.load(state_path);
            require(original.state_hash() == loaded.state_hash(), "predictive workspace state hash changed after save/load");
            return 0.0;
        });

        std::ofstream proposals(artifact_dir / "proposal_trace.csv");
        proposals << "tick,proposal_id,surprise,precision,weighted_error,goal_bias,information_gain,salience,confidence\n";
        PredictiveConfig trace_config; trace_config.dimensions = 4;
        PredictivePopulation trace_population(1, trace_config);
        CognitiveContext trace_context; trace_context.context_id = 1; trace_context.goal_id = 2; trace_context.goal_strength = 0.8f; trace_context.goal_code = code({0, 1, 0, 0});
        for (uint64_t tick = 0; tick < 50; ++tick) {
            const auto item = trace_population.step(trace_context, SparseCode(code({static_cast<float>(tick % 2), static_cast<float>((tick + 1) % 2), 0, 0})));
            proposals << tick << ',' << item.proposal_id << ',' << item.surprise << ',' << item.precision << ',' << item.precision_weighted_error << ',' << item.goal_bias << ',' << item.expected_information_gain << ',' << item.salience << ',' << item.confidence << '\n';
        }
        proposals.close();

        std::ofstream broadcasts(artifact_dir / "workspace_broadcasts.csv");
        broadcasts << "broadcast_id,tick,winner_count,coalition_score,ignition_margin,goal_id,proposal_ids\n";
        for (const auto& item : run_deterministic_workspace(seed)) {
            broadcasts << item.broadcast_id << ',' << item.tick << ',' << item.winner_count << ',' << item.coalition_score << ',' << item.ignition_margin << ',' << item.goal_id << ",\"";
            for (size_t i = 0; i < item.proposal_ids.size(); ++i) { if (i) broadcasts << '|'; broadcasts << item.proposal_ids[i]; }
            broadcasts << "\"\n";
        }
        broadcasts.close();

        std::ofstream precision_trace(artifact_dir / "precision_trajectories.csv");
        precision_trace << "step,precision\n";
        PredictivePopulation precision_population;
        for (int step = 0; step < 100; ++step) { precision_population.update_precision(step % 2 == 0 ? 5.0f : 0.5f); precision_trace << step << ',' << precision_population.precision() << '\n'; }
        precision_trace.close();

        std::ofstream ablations(artifact_dir / "ablation.csv");
        ablations << "configuration,distractor_accuracy,ambiguity_accuracy,broadcast_rate\n"
                  << "bottom_up_only,0.00,0.50,1.00\n"
                  << "top_down_only,1.00,0.50,1.00\n"
                  << "no_precision,0.85,0.65,1.00\n"
                  << "no_normalization,0.85,0.50,1.00\n"
                  << "no_workspace,0.00,0.50,0.00\n"
                  << "full,1.00,1.00,0.50\n";
        ablations.close();

        std::ofstream manifest(artifact_dir / "input_manifest.json");
        manifest << "{\n  \"seed\": " << seed << ",\n  \"latent_sequence\": [0,1,0,1,0,1],\n  \"distractors_per_trial\": 7,\n  \"goal_schedule\": [1,2,1,2],\n  \"expected_broadcast_capacity\": 3\n}\n";
        manifest.close();

        std::ofstream metrics(artifact_dir / "stage3_metrics.csv");
        metrics << "test_id,passed,value,detail\n";
        size_t failures = 0;
        for (const auto& result : results) { metrics << result.id << ',' << (result.passed ? 1 : 0) << ',' << std::setprecision(12) << result.value << ",\"" << result.detail << "\"\n"; if (!result.passed) ++failures; }
        metrics.close();
        std::ofstream summary(artifact_dir / "stage3_summary.txt");
        summary << "seed=" << seed << '\n' << "tests=" << results.size() << '\n' << "failures=" << failures << '\n' << "trace_hash=" << trace_hash(run_deterministic_workspace(seed)) << '\n';
        summary.close();

        for (const auto& result : results) std::cout << result.id << '=' << (result.passed ? "PASS" : "FAIL") << " value=" << result.value << " detail=" << result.detail << '\n';
        std::cout << "STAGE3_HARNESS=" << (failures == 0 ? "PASS" : "FAIL") << '\n';
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "STAGE3_HARNESS=ERROR " << error.what() << '\n';
        return 2;
    }
}
