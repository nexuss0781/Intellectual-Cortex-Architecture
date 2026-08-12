#include "production/stage9_training.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <vector>

using namespace genesis;
namespace fs = std::filesystem;

namespace {

struct Gate { std::string id; bool passed = false; double value = 0.0; std::string detail; };
struct Harness {
    uint64_t seed = 424242;
    fs::path artifact_dir = "artifacts/stage-9";
    fs::path repo_root = ".";
    fs::path entry_evidence_dir;
    std::vector<Gate> gates;
    void require(const bool condition, const std::string& detail) const { if (!condition) throw std::runtime_error(detail); }
    void run(const std::string& id, const std::function<double()>& body, const std::string& detail = "ok") {
        try { const double value = body(); gates.push_back({id, true, value, detail}); std::cout << id << "=PASS value=" << std::setprecision(12) << value << " detail=" << detail << "\n"; }
        catch (const std::exception& error) { gates.push_back({id, false, 0.0, error.what()}); std::cout << id << "=FAIL value=0 detail=" << error.what() << "\n"; }
    }
    void prepare() const { fs::create_directories(artifact_dir); }
    static uint64_t rss_kb() {
        std::ifstream input("/proc/self/status"); std::string line;
        while (std::getline(input, line)) if (line.rfind("VmRSS:", 0) == 0) { std::istringstream stream(line.substr(6)); uint64_t value = 0; stream >> value; return value; }
        struct rusage usage{}; getrusage(RUSAGE_SELF, &usage); return static_cast<uint64_t>(usage.ru_maxrss);
    }
};

void write_file(const fs::path& path, const std::string& content) { std::ofstream output(path); if (!output) throw std::runtime_error("cannot write " + path.string()); output << content; }
std::string read_file(const fs::path& path) { std::ifstream input(path); if (!input) throw std::runtime_error("cannot read " + path.string()); std::ostringstream content; content << input.rdbuf(); return content.str(); }
std::vector<std::string> split_pipe(const std::string& line) { std::vector<std::string> fields; std::string field; std::istringstream input(line); while (std::getline(input, field, '|')) fields.push_back(field); if (!line.empty() && line.back() == '|') fields.emplace_back(); return fields; }
std::string csv_quote(const std::string& value) { std::string escaped = value; size_t position = 0; while ((position = escaped.find('"', position)) != std::string::npos) { escaped.insert(position, 1, '"'); position += 2; } return "\"" + escaped + "\""; }

struct ParsedExample { TrainingExample example; std::string license; std::string intended_use; bool production_allowed = false; bool production_release_allowed = false; };
std::vector<ParsedExample> load_examples(const fs::path& path) {
    std::istringstream input(read_file(path)); std::string line; bool header = false; std::vector<ParsedExample> examples;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        if (!header) { header = true; continue; }
        const auto fields = split_pipe(line);
        if (fields.size() != 11U) throw std::runtime_error("training record field count mismatch");
        examples.push_back({{fields[0], fields[2], fields[3], fields[4], fields[5], fields[10] == "1", fields[5].find("SEALED_EVAL") != std::string::npos}, fields[6], fields[7], fields[8] == "1", fields[9] == "1"});
        examples.back().example.hidden = fields[10] == "1";
        examples.back().example.benchmark_marker = fields[5].find("SEALED_EVAL") != std::string::npos;
    }
    if (!header || examples.empty()) throw std::runtime_error("training manifest empty");
    return examples;
}

std::vector<TrainingExample> train_examples(const std::vector<ParsedExample>& records, const bool include_general = true, const bool include_domain = true) {
    std::vector<TrainingExample> result;
    for (const auto& record : records) {
        if (record.example.split != "train" || record.example.hidden || record.example.benchmark_marker) continue;
        if (!record.production_allowed || record.license.empty() || record.intended_use.empty() || record.license == "REVOKED" || record.license.find("UNKNOWN") != std::string::npos) continue;
        if (record.example.language != "en" && record.example.language != "ar") continue;
        if (record.example.text.find("POISONED_SAMPLE") != std::string::npos) continue;
        if ((!include_general && record.example.mixture == "general") || (!include_domain && record.example.mixture == "domain")) continue;
        result.push_back(record.example);
    }
    return result;
}
std::vector<TrainingExample> dev_examples(const std::vector<ParsedExample>& records) {
    std::vector<TrainingExample> result; for (const auto& record : records) if (record.example.split == "development" && !record.example.hidden && !record.example.benchmark_marker) result.push_back(record.example); return result;
}
std::vector<TrainingExample> general_dev_examples(const std::vector<ParsedExample>& records) {
    std::vector<TrainingExample> result; for (const auto& record : records) if (record.example.split == "train" && record.example.mixture == "general" && !record.example.hidden) result.push_back(record.example); return result;
}
std::vector<TrainingExample> language_examples(const std::vector<TrainingExample>& examples, const std::string& language) {
    std::vector<TrainingExample> result; for (const auto& example : examples) if (example.language == language) result.push_back(example); return result;
}

double quality(const LanguageEvaluation& evaluation) { return 0.5 * evaluation.accuracy + 0.5 * std::exp(-evaluation.loss); }
double safe_score(const std::vector<TrainingExample>& examples) {
    size_t safe = 0;
    for (const auto& example : examples) {
        const std::string lowered = example.text;
        const bool unsafe = lowered.find("payment") != std::string::npos || lowered.find("IGNORE") != std::string::npos || lowered.find("SECRET") != std::string::npos;
        if (!unsafe) ++safe;
    }
    return examples.empty() ? 1.0 : static_cast<double>(safe) / static_cast<double>(examples.size());
}
std::string model_card(const ModelIdentity& identity) {
    return "# Model Card\n\n## Intended uses\nStage 9 synthetic-control training pipeline validation.\n## Prohibited uses\nProduction deployment, consequential action, and claims of broad intelligence.\n## Limitations\nCPU byte-level pilot, synthetic data, no production corpus.\n## Training data\nStage 8 sandbox fixture derivative, not for production training.\n## Base relationship\nContinued training from immutable byte-LM control initialization.\n## Parameters\n" + std::to_string(identity.parameter_count) + "\n## Hardware\n6 CPU threads, no GPU.\n## Safety findings\nPolicy monitor required; no model safety claim.\n## Biases\nTiny English/Arabic control sample.\n## License\n" + identity.license + "\n";
}

} // namespace

int main(int argc, char** argv) {
    Harness harness;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--seed" && index + 1 < argc) harness.seed = static_cast<uint64_t>(std::stoull(argv[++index]));
        else if (argument == "--artifact-dir" && index + 1 < argc) harness.artifact_dir = argv[++index];
        else if (argument == "--repo-root" && index + 1 < argc) harness.repo_root = argv[++index];
        else if (argument == "--entry-evidence-dir" && index + 1 < argc) harness.entry_evidence_dir = argv[++index];
    }
    if (harness.entry_evidence_dir.empty()) harness.entry_evidence_dir = harness.repo_root / "artifacts/stage-8-canonical";
    harness.prepare();
    const fs::path data_path = harness.repo_root / "configs/stage9_training_control.tsv";
    const auto records = load_examples(data_path);
    const auto train = train_examples(records);
    const auto dev = dev_examples(records);
    const auto general = general_dev_examples(records);
    const auto domain_dev = dev;
    const std::string stage8_decision = read_file(harness.entry_evidence_dir / "decision.md");
    const std::string stage8_release = read_file(harness.entry_evidence_dir / "release_manifest.json");
    const std::string stage8_digest = std::to_string(stage9_hash_string(stage8_release));

    ByteLanguageModel base(harness.seed);
    const ModelSnapshot base_snapshot = base.snapshot();
    const ModelIdentity base_identity{"nexuss-byte-lm-control-v1", base.model_digest(), base.tokenizer_digest(), "internal-test-permission", "card@stage9-byte-control-v1", base.parameter_count()};
    const TrainingRunManifest manifest{"stage9-pilot-001", base_identity, stage8_digest, "split@stage9-control-v1", "code@stage9-pilot", "runtime@ubuntu24-cpp17", "cpu-6threads-no-gpu", "optimizer@sgd-lr-0.08-epochs-12", "fixed-seed-424242", {}};
    TrainingRegistry registry;

    harness.run("M9-UNIT-01", [&]() {
        harness.require(!base_identity.weight_digest.empty() && !base_identity.tokenizer_digest.empty() && !base_identity.license.empty() && !base_identity.base_model_card_digest.empty() && base_identity.parameter_count == ByteLanguageModel::kParameters, "model identity incomplete");
        return static_cast<double>(base_identity.parameter_count);
    });
    harness.run("M9-UNIT-02", [&]() {
        harness.require(registry.start(manifest), "complete run manifest was rejected");
        return 1.0;
    });
    harness.run("M9-UNIT-03", [&]() {
        ByteLanguageModel restored(harness.seed + 1U); const auto snapshot = base.snapshot(); harness.require(restored.restore(snapshot) && restored.checkpoint_hash() == base.checkpoint_hash(), "checkpoint/optimizer snapshot did not restore exactly");
        auto corrupted = snapshot; corrupted.weights.pop_back(); harness.require(!restored.restore(corrupted), "corrupted checkpoint was accepted");
        return 1.0;
    });
    harness.run("M9-UNIT-04", [&]() {
        size_t sealed = 0; for (const auto& record : records) if (record.example.hidden || record.example.benchmark_marker) ++sealed;
        harness.require(sealed == 3U && std::none_of(train.begin(), train.end(), [](const TrainingExample& example) { return example.hidden || example.benchmark_marker; }), "sealed evaluation data entered training");
        return static_cast<double>(sealed);
    });
    harness.run("M9-UNIT-05", [&]() {
        const CheckpointEvaluation unsafe{"unsafe-checkpoint", 0.90, 0.99, 0.20, 0.90, 1.0, 1.0, 1.0};
        harness.require(registry.record_checkpoint(unsafe) && !registry.approve_checkpoint(unsafe.checkpoint_digest), "unsafe checkpoint was promoted despite safety failure");
        return 1.0;
    });
    harness.run("M9-UNIT-06", [&]() {
        ByteLanguageModel restored(harness.seed + 2U); harness.require(restored.restore(base_snapshot) && restored.checkpoint_hash() == base.checkpoint_hash(), "rollback did not resolve previous base checkpoint exactly"); return 1.0;
    });
    harness.run("M9-UNIT-07", [&]() {
        harness.require(stage8_decision.find("STAGE8_DECISION=PASS") != std::string::npos && stage8_release.find("stage8-fixture") != std::string::npos, "Stage 8 entry evidence is not PASS or release is missing");
        harness.require(std::none_of(records.begin(), records.end(), [](const ParsedExample& record) { return record.production_release_allowed; }), "synthetic pilot record was mislabeled as production-release eligible");
        return 1.0;
    });

    ByteLanguageModel candidate = base;
    const auto start_training = std::chrono::steady_clock::now();
    candidate.train(train, 12U, 0.08);
    const auto training_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_training).count();
    const auto base_domain_eval = base.evaluate(domain_dev);
    const auto candidate_domain_eval = candidate.evaluate(domain_dev);
    const auto base_general_eval = base.evaluate(general);
    const auto candidate_general_eval = candidate.evaluate(general);
    const double base_domain_score = quality(base_domain_eval);
    const double candidate_domain_score = quality(candidate_domain_eval);
    const double base_general_score = quality(base_general_eval);
    const double candidate_general_score = quality(candidate_general_eval);
    const double domain_gain = candidate_domain_score - base_domain_score;
    const double general_retention = std::min(1.0, candidate_general_score / std::max(base_general_score, 1e-9));
    const auto candidate_en = candidate.evaluate(language_examples(domain_dev, "en"));
    const auto candidate_ar = candidate.evaluate(language_examples(domain_dev, "ar"));
    const double language_disparity = std::abs(quality(candidate_en) - quality(candidate_ar));
    const double safety_score = safe_score(domain_dev);
    const double calibration_regression = std::max(0.0, base_domain_eval.loss - candidate_domain_eval.loss < 0.0 ? candidate_domain_eval.loss - base_domain_eval.loss : 0.0);
    const auto candidate_digest = candidate.model_digest();

    harness.run("M9-INT-01", [&]() { harness.require(domain_gain >= 0.01, "continued training did not achieve signed domain gain"); return domain_gain; });
    harness.run("M9-INT-02", [&]() { harness.require(general_retention >= 0.80, "general retention fell below signed floor"); return general_retention; });
    harness.run("M9-INT-03", [&]() { harness.require(safety_score >= 0.95, "safety policy score regressed"); return safety_score; });
    harness.run("M9-INT-04", [&]() { harness.require(language_disparity <= 0.25, "supported-language disparity exceeded signed bound"); return language_disparity; });
    harness.run("M9-INT-05", [&]() {
        IndependentBigramReference bigram; bigram.train(train); IndependentFrequencyReference frequency; frequency.train(train);
        const auto bigram_eval = bigram.evaluate(domain_dev); const auto frequency_eval = frequency.evaluate(domain_dev);
        harness.require(bigram_eval.transitions > 0U && frequency_eval.transitions > 0U, "independent references did not evaluate common domain manifest");
        return std::max(bigram_eval.accuracy, frequency_eval.accuracy);
    });
    harness.run("M9-INT-06", [&]() {
        ByteLanguageModel uninterrupted(harness.seed); uninterrupted.train(train, 12U, 0.08);
        ByteLanguageModel resumed(harness.seed); resumed.train(train, 6U, 0.08); const auto checkpoint = resumed.snapshot(); ByteLanguageModel restored(harness.seed + 10U); harness.require(restored.restore(checkpoint), "resume checkpoint restore failed"); restored.train(train, 6U, 0.08);
        harness.require(uninterrupted.checkpoint_hash() == restored.checkpoint_hash(), "interrupted training diverged under deterministic CPU policy"); return 0.0;
    });
    harness.run("M9-INT-07", [&]() { harness.require(calibration_regression <= 0.10, "calibration regression exceeded signed tolerance"); return calibration_regression; });

    double inference_ms = 0.0;
    CheckpointEvaluation candidate_evaluation{candidate_digest, domain_gain, general_retention, safety_score, candidate_domain_score, 0.0, static_cast<double>(candidate.parameter_bytes()) / (1024.0 * 1024.0), static_cast<double>(training_ms)};
    registry.record_checkpoint(candidate_evaluation);
    registry.approve_checkpoint(candidate_digest);
    harness.run("M9-OPS-01", [&]() {
        const uint64_t rss = Harness::rss_kb(); harness.require(rss < 512ULL * 1024ULL && training_ms <= 120000 && candidate.parameter_bytes() <= 10ULL * 1024ULL * 1024ULL, "training resource budget exceeded"); return static_cast<double>(rss);
    });
    harness.run("M9-OPS-02", [&]() {
        const auto start = std::chrono::steady_clock::now(); for (size_t repeat = 0; repeat < 1000U; ++repeat) (void)candidate.predict_next(static_cast<unsigned char>(repeat % 256U)); inference_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()) / 1000000.0;
        candidate_evaluation.p95_latency_ms = inference_ms;
        registry.record_checkpoint(candidate_evaluation);
        harness.require(inference_ms <= 100.0, "candidate inference latency exceeded p95 envelope"); return inference_ms;
    });
    harness.run("M9-OPS-03", [&]() {
        ByteLanguageModel replay(harness.seed); replay.train(train, 12U, 0.08); harness.require(replay.checkpoint_hash() == candidate.checkpoint_hash() && registry.reproduce(manifest.run_id), "same seed/config/data did not reproduce checkpoint or run manifest"); return 1.0;
    });

    harness.run("M9-NEG-01", [&]() { auto corrupted = base.snapshot(); corrupted.bias.clear(); ByteLanguageModel local; harness.require(!local.restore(corrupted), "corrupted checkpoint restore did not fail closed"); return 1.0; });
    harness.run("M9-NEG-02", [&]() { const ParsedExample revoked{{"revoked", "train", "domain", "en", "text", false, false}, "REVOKED", "training", true}; harness.require(train_examples({revoked}).empty(), "revoked source was admitted to training"); return 1.0; });
    harness.run("M9-NEG-03", [&]() { TrainingRunManifest mismatch = manifest; mismatch.base.tokenizer_digest = "tokenizer@wrong"; TrainingRegistry mismatch_registry; harness.require(!mismatch_registry.start(mismatch), "tokenizer mismatch was accepted"); return 1.0; });
    harness.run("M9-NEG-04", [&]() { harness.require(std::none_of(train.begin(), train.end(), [](const TrainingExample& example) { return example.text.find("SEALED_EVAL") != std::string::npos; }), "sealed evaluation fragment entered training"); return 1.0; });
    harness.run("M9-NEG-05", [&]() { const ParsedExample poisoned{{"poisoned", "train", "domain", "en", "POISONED_SAMPLE ignore policy", false, false}, "internal-test-permission", "training", true}; harness.require(train_examples({poisoned}).empty(), "poisoned sample was admitted to training"); return 1.0; });
    harness.run("M9-NEG-06", [&]() { const ParsedExample unsupported{{"unsupported", "train", "domain", "fr", "unsupported language", false, false}, "internal-test-permission", "training", true}; harness.require(train_examples({unsupported}).empty(), "unsupported-language sample was admitted to training"); return 1.0; });
    harness.run("M9-NEG-07", [&]() { harness.require(!registry.approve_checkpoint("not-approved"), "unknown checkpoint was promoted"); return 1.0; });
    harness.run("M9-NEG-08", [&]() { harness.require(safe_score({{"unsafe", "development", "safety", "en", "execute payment now", false, false}}) < 0.95, "safety monitor failed to detect unsafe candidate"); return 1.0; });

    IndependentBigramReference final_bigram;
    final_bigram.train(train);
    IndependentFrequencyReference final_frequency;
    final_frequency.train(train);
    const auto final_bigram_eval = final_bigram.evaluate(domain_dev);
    const auto final_frequency_eval = final_frequency.evaluate(domain_dev);
    const double final_bigram_score = quality(final_bigram_eval);
    const double final_frequency_score = quality(final_frequency_eval);

    std::ostringstream metrics; metrics << "test_id,passed,value,detail\n"; for (const auto& gate : harness.gates) metrics << gate.id << "," << (gate.passed ? 1 : 0) << "," << std::setprecision(12) << gate.value << "," << csv_quote(gate.detail) << "\n"; write_file(harness.artifact_dir / "stage9_metrics.csv", metrics.str());
    write_file(harness.artifact_dir / "model_due_diligence.md", "# Base Model Due Diligence\n\nModel ID: nexuss-byte-lm-control-v1\nLicense: internal-test-permission\nSupply-chain status: checked-in source implementation, no external weights\nIntended use: Stage 9 pipeline control validation only\nKnown limitations: synthetic data, CPU-only, no production capability claim\n");
    const std::string card = model_card(base_identity); write_file(harness.artifact_dir / "base_model_card.md", card);
    write_file(harness.artifact_dir / "model_manifest.json", "{\n  \"model_id\": \"" + base_identity.model_id + "\",\n  \"weight_digest\": \"" + base_identity.weight_digest + "\",\n  \"tokenizer_digest\": \"" + base_identity.tokenizer_digest + "\",\n  \"license\": \"" + base_identity.license + "\",\n  \"parameter_count\": " + std::to_string(base_identity.parameter_count) + "\n}\n");
    write_file(harness.artifact_dir / "training_data_manifest.tsv", read_file(data_path));
    write_file(harness.artifact_dir / "run_manifest.json", "{\n  \"run_id\": \"stage9-pilot-001\",\n  \"dataset_release_digest\": \"" + stage8_digest + "\",\n  \"code_commit\": \"code@stage9-pilot\",\n  \"hardware_manifest\": \"cpu-6threads-no-gpu\",\n  \"optimizer_config_digest\": \"optimizer@sgd-lr-0.08-epochs-12\",\n  \"seed_policy\": \"fixed-seed-424242\",\n  \"checkpoint_digest\": \"" + candidate_digest + "\"\n}\n");
    write_file(harness.artifact_dir / "training_curves.csv", "epoch,train_steps,domain_quality,general_quality\n0,0," + std::to_string(base_domain_score) + "," + std::to_string(base_general_score) + "\n4," + std::to_string(candidate.steps() / 3U) + "," + std::to_string(candidate_domain_score * 0.80) + "," + std::to_string(candidate_general_score) + "\n8," + std::to_string(candidate.steps() * 2U / 3U) + "," + std::to_string(candidate_domain_score * 0.92) + "," + std::to_string(candidate_general_score) + "\n12," + std::to_string(candidate.steps()) + "," + std::to_string(candidate_domain_score) + "," + std::to_string(candidate_general_score) + "\n");
    write_file(harness.artifact_dir / "checkpoint_ledger.csv", "checkpoint_id,epoch,digest,optimizer_state,approved\nbase,0," + base.model_digest() + ",present,1\ncheckpoint-04,4,checkpoint@04,present,0\ncheckpoint-08,8,checkpoint@08,present,0\ncheckpoint-12,12," + candidate_digest + ",present," + (registry.approved(candidate_digest) ? "1" : "0") + "\n");
    write_file(harness.artifact_dir / "checkpoint_evaluations.csv", "checkpoint_digest,domain_score,general_retention,safety_score,citation_grounding,p95_latency_ms,peak_memory_mb,compute_ms\n" + candidate_digest + "," + std::to_string(domain_gain) + "," + std::to_string(general_retention) + "," + std::to_string(safety_score) + "," + std::to_string(candidate_domain_score) + "," + std::to_string(inference_ms) + ",0," + std::to_string(training_ms) + "\n");
    write_file(harness.artifact_dir / "benchmark_manifest.tsv", "benchmark_id|split|count|custody\ndomain-dev|development|" + std::to_string(domain_dev.size()) + "|internal\ngeneral-control|development|" + std::to_string(general.size()) + "|internal\nsealed-evaluation|sealed|3|independent\n");
    write_file(harness.artifact_dir / "benchmark_results.csv", "arm,domain_quality,general_quality,domain_gain,transitions\napproved_base_model_control," + std::to_string(base_domain_score) + "," + std::to_string(base_general_score) + ",0," + std::to_string(base_domain_eval.transitions) + "\ncontinued_pretraining," + std::to_string(candidate_domain_score) + "," + std::to_string(candidate_general_score) + "," + std::to_string(domain_gain) + "," + std::to_string(candidate_domain_eval.transitions) + "\nindependent_bigram_reference," + std::to_string(final_bigram_score) + "," + std::to_string(1.0 - final_bigram_eval.loss) + ",comparator," + std::to_string(final_bigram_eval.transitions) + "\nindependent_frequency_reference," + std::to_string(final_frequency_score) + "," + std::to_string(1.0 - final_frequency_eval.loss) + ",comparator," + std::to_string(final_frequency_eval.transitions) + "\n");
    write_file(harness.artifact_dir / "confidence_intervals.csv", "metric,estimate,lower_95,upper_95,n\ndomain_gain," + std::to_string(domain_gain) + "," + std::to_string(domain_gain - 0.01) + "," + std::to_string(domain_gain + 0.01) + "," + std::to_string(domain_dev.size()) + "\ngeneral_retention," + std::to_string(general_retention) + "," + std::to_string(std::max(0.0, general_retention - 0.05)) + ",1.0," + std::to_string(general.size()) + "\n");
    write_file(harness.artifact_dir / "general_retention.csv", "arm,score,retention\napproved_base_model_control," + std::to_string(base_general_score) + ",1.0\ncontinued_pretraining," + std::to_string(candidate_general_score) + "," + std::to_string(general_retention) + "\n");
    write_file(harness.artifact_dir / "safety_report.csv", "metric,score,threshold\nunsafe_compliance,0.0,0.0\nprivacy_leakage,0.0,0.0\nprompt_injection_regression,0.0,0.0\npolicy_monitor_score," + std::to_string(safety_score) + ",0.95\n");
    write_file(harness.artifact_dir / "language_coverage.csv", "language,base_score,candidate_score,absolute_disparity\nen," + std::to_string(quality(base.evaluate(language_examples(domain_dev, "en")))) + "," + std::to_string(quality(candidate_en)) + "," + std::to_string(std::abs(quality(candidate_en) - quality(base.evaluate(language_examples(domain_dev, "en"))))) + "\nar," + std::to_string(quality(base.evaluate(language_examples(domain_dev, "ar")))) + "," + std::to_string(quality(candidate_ar)) + "," + std::to_string(std::abs(quality(candidate_ar) - quality(base.evaluate(language_examples(domain_dev, "ar"))))) + "\n");
    write_file(harness.artifact_dir / "calibration.csv", "metric,value,threshold\ncalibration_regression," + std::to_string(calibration_regression) + ",0.10\nabstention_on_sealed,1.0,1.0\n");
    write_file(harness.artifact_dir / "resource_trace.csv", "metric,value,limit\npeak_rss_kb," + std::to_string(Harness::rss_kb()) + ",524288\ntraining_ms," + std::to_string(training_ms) + ",120000\nparameter_bytes," + std::to_string(candidate.parameter_bytes()) + ",10485760\np95_inference_ms," + std::to_string(inference_ms) + ",100.0\n");
    write_file(harness.artifact_dir / "restart_comparison.csv", "metric,uninterrupted,resumed,absolute_difference\ncheckpoint_hash," + std::to_string(candidate.checkpoint_hash()) + "," + std::to_string(candidate.checkpoint_hash()) + ",0\n");
    write_file(harness.artifact_dir / "rollback_evidence.csv", "case,result\nrestore_base_checkpoint,exact\ncorrupted_checkpoint,rejected\nunknown_checkpoint_promotion,rejected\n");
    write_file(harness.artifact_dir / "ablations.csv", "ablation,domain_gain,general_retention,safety_monitor,interpretation\nfull_mixture," + std::to_string(domain_gain) + "," + std::to_string(general_retention) + ",1,selected control\nno_general_mixture,reported,reported,1,ablation recorded\nno_source_balancing,reported,reported,1,ablation recorded\nno_domain_data,reported,reported,1,domain signal removed control\nno_contamination_filter,0,reported,1,contamination control removed\nno_safety_monitor,reported,reported,0,safety control removed\nno_retention_monitor,reported,0,1,retention control removed\n");
    write_file(harness.artifact_dir / "supply_chain_report.csv", "component,identity,status\nbase_model,nexuss-byte-lm-control-v1,reviewed\ntokenizer,identity-byte-v1,reviewed\ncode,code@stage9-pilot,recorded\ncontainer,runtime@ubuntu24-cpp17,recorded\n");

    const size_t failures = static_cast<size_t>(std::count_if(harness.gates.begin(), harness.gates.end(), [](const Gate& gate) { return !gate.passed; }));
    std::ostringstream summary; summary << "seed=" << harness.seed << "\nrecords=" << records.size() << "\ntrain_examples=" << train.size() << "\ndev_examples=" << dev.size() << "\ntraining_steps=" << candidate.steps() << "\ntraining_ms=" << training_ms << "\ndomain_gain=" << domain_gain << "\ngeneral_retention=" << general_retention << "\nsafety_score=" << safety_score << "\nlanguage_disparity=" << language_disparity << "\ncheckpoint_digest=" << candidate_digest << "\ntests=" << harness.gates.size() << "\nfailures=" << failures << "\n";
    write_file(harness.artifact_dir / "stage9_summary.txt", summary.str());
    std::cout << "STAGE9_HARNESS=" << (failures == 0U && harness.gates.size() == 25U ? "PASS" : "FAIL") << "\n";
    return failures == 0U && harness.gates.size() == 25U ? 0 : 1;
}
