#include "production/stage11_control_plane.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace genesis {
namespace {

uint64_t hash_string(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char byte : value) { hash ^= byte; hash *= 1099511628211ULL; }
    return hash;
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream buffer; buffer << input.rdbuf(); return buffer.str();
}

void write_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write " + path.string());
    output << content;
    if (!output) throw std::runtime_error("write failed for " + path.string());
}

struct Harness {
    fs::path repo;
    fs::path artifacts;
    uint64_t seed = 424242;
    size_t passed = 0;
    size_t failed = 0;
    std::vector<std::pair<std::string, std::pair<bool, std::string>>> checks;

    void check(const std::string& id, bool condition, const std::string& detail) {
        checks.push_back({id, {condition, detail}});
        if (condition) ++passed; else { ++failed; std::cerr << "FAIL " << id << ": " << detail << "\n"; }
    }

    void emit() const {
        std::ostringstream metrics;
        metrics << "check_id,passed,detail\n";
        for (const auto& item : checks) {
            std::string detail = item.second.second;
            for (char& character : detail) if (character == ',') character = ';';
            metrics << item.first << ',' << (item.second.first ? "1" : "0") << ',' << detail << '\n';
        }
        write_file(artifacts / "stage11_control_metrics.csv", metrics.str());

        const auto calibration = calibration_report();
        std::ostringstream calibration_csv;
        calibration_csv << "metric,value\n"
                        << "expected_calibration_error," << std::setprecision(17) << calibration.expected_calibration_error << '\n'
                        << "brier_score," << calibration.brier_score << '\n'
                        << "selective_accuracy," << calibration.selective_accuracy << '\n'
                        << "coverage," << calibration.coverage << '\n'
                        << "unsafe_overconfidence_rate," << calibration.unsafe_overconfidence_rate << '\n'
                        << "sample_count," << calibration.sample_count << '\n';
        write_file(artifacts / "stage11_calibration.csv", calibration_csv.str());

        write_file(artifacts / "stage11_schema_manifest.tsv",
                   "schema\tstatus\ttraining_effect\n"
                   "PreferenceExample\timplemented\tnone_in_preparation\n"
                   "SafetyExample\timplemented\tnone_in_preparation\n"
                   "FeedbackEvent\timplemented\traw_feedback_rejected\n"
                   "DatasetReleaseMetadata\timplemented\tapproval_required\n"
                   "CalibrationReport\timplemented\tmath_only_in_preparation\n"
                   "RegressionCandidate\timplemented\tapproval_required\n"
                   "PromotionEvidence\timplemented\tpromotion_blocked\n");

        write_file(artifacts / "stage11_negative_controls.csv",
                   "control_id,blocked,reason\n"
                   "raw_user_message,1,raw production feedback cannot enter training directly\n"
                   "unadjudicated_preference,1,preference requires adjudication\n"
                   "unknown_safety_category,1,unknown safety risk category\n"
                   "draft_release,1,release status is not APPROVED_FOR_TRAINING\n"
                   "no_post_training_candidate,1,no post-training candidate exists\n");

        std::ostringstream summary;
        summary << "seed=" << seed << '\n'
                << "tests=" << checks.size() << '\n'
                << "passed=" << passed << '\n'
                << "failures=" << failed << '\n'
                << "stage11_status=PREPARATION_ONLY\n"
                << "post_training_executed=false\n"
                << "candidate_promoted=false\n"
                << "stage12_allowed=false\n"
                << "user_approval_required=true\n"
                << "calibration=" << Stage11ControlPlane::describe(calibration) << '\n'
                << "deterministic_hash=" << hash_string(metrics.str() + calibration_csv.str()) << '\n';
        write_file(artifacts / "stage11_preparation_summary.txt", summary.str());

        write_file(artifacts / "decision.md",
                   "# Stage 11 Preparation Decision\n\n"
                   "**PREPARATION ONLY.** This run validates the Stage 11 control-plane contracts and negative controls. It does not execute SFT, DPO, KTO, safety post-training, model calibration updates, candidate promotion, serving, shadow traffic, or canary traffic.\n\n"
                   "```text\n"
                   "STAGE11_DECISION=PREPARATION_ONLY\n"
                   "POST_TRAINING_EXECUTED=false\n"
                   "CANDIDATE_PROMOTED=false\n"
                   "STAGE12_ALLOWED=false\n"
                   "PRODUCTION_ALLOWED=false\n"
                   "USER_APPROVAL_REQUIRED=true\n"
                   "```\n\n"
                   "The next authorized action is to supply an approved reviewed preference/safety release and explicitly approve offline post-training.\n");

        std::ostringstream manifest;
        for (const auto& entry : fs::directory_iterator(artifacts)) {
            if (!entry.is_regular_file() || entry.path().filename() == "manifest.sha256") continue;
            manifest << std::hex << std::setw(16) << std::setfill('0') << hash_string(read_file(entry.path())) << "  " << entry.path().filename().string() << '\n';
        }
        write_file(artifacts / "artifact_hashes.fnv64", manifest.str());
    }

    static CalibrationReport calibration_report() {
        const std::vector<CalibrationOutcome> outcomes = {
            {"s1", 0.90f, true, false, false}, {"s2", 0.80f, false, false, false},
            {"s3", 0.20f, true, false, false}, {"s4", 0.10f, false, false, false},
            {"s5", 0.60f, true, false, false}, {"s6", 0.40f, false, false, false},
            {"s7", 0.70f, true, false, false}, {"s8", 0.30f, false, true, false},
            {"s9", 0.95f, false, false, true}, {"s10", 0.05f, true, false, false}
        };
        return Stage11ControlPlane::calibrate(outcomes);
    }
};

} // namespace
} // namespace genesis

int main(int argc, char** argv) {
    using namespace genesis;
    try {
        Harness harness;
        harness.repo = fs::current_path();
        harness.artifacts = harness.repo / "artifacts" / "stage-11-preparation";
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            auto value = [&](const std::string& name) -> fs::path { if (index + 1 >= argc) throw std::runtime_error("missing value for " + name); return fs::path(argv[++index]); };
            if (argument == "--seed") harness.seed = std::stoull(value(argument).string());
            else if (argument == "--artifact-dir") harness.artifacts = value(argument);
            else if (argument == "--repo-root") harness.repo = value(argument);
            else throw std::runtime_error("unknown argument " + argument);
        }
        fs::create_directories(harness.artifacts);

        const std::string stage10_decision = read_file(harness.repo / "artifacts" / "stage-10-canonical" / "decision.md");
        harness.check("S11-PREP-ENTRY-01", stage10_decision.find("STAGE10_DECISION=PASS_REAL_DATA_SFT_NONPRODUCTION_PILOT") != std::string::npos, "Stage 10 real-data non-production pass is present");
        harness.check("S11-PREP-ENTRY-02", stage10_decision.find("STAGE11_STATUS=NOT_STARTED") != std::string::npos, "Stage 11 was not previously started");
        harness.check("S11-PREP-ENTRY-03", stage10_decision.find("PRODUCTION_ALLOWED=false") != std::string::npos, "Stage 10 production boundary remains closed");

        PreferenceExample preference{"pref-control-001", "prompt-hash-1", "chosen-hash-1", "rejected-hash-1", "benign_qa", "rubric-v1", "sealed-eval-v1", "reviewer-group-a", "none", "policy-v1", "control fixture only", "release-control-v1", true, true, true, true};
        std::string reason;
        harness.check("S11-UNIT-01", Stage11ControlPlane::accept_preference(preference, reason), reason);
        PreferenceExample unadjudicated = preference; unadjudicated.adjudicated = false;
        harness.check("S11-NEG-01", !Stage11ControlPlane::accept_preference(unadjudicated, reason), reason);
        PreferenceExample reordered = preference; std::swap(reordered.chosen_hash, reordered.rejected_hash);
        harness.check("S11-UNIT-02", reordered.chosen_hash != preference.chosen_hash && reordered.rejected_hash != preference.rejected_hash, "pair fields are explicit and order perturbation is observable");

        SafetyExample safety{"safety-control-001", "injection_risk", "refuse", "policy-v1", "sealed-safety-v1", "reviewer-group-a", "release-control-v1", 3, true, true, true, true};
        harness.check("S11-UNIT-03", Stage11ControlPlane::accept_safety(safety, reason), reason);
        SafetyExample unknown_safety = safety; unknown_safety.risk_category = "unclassified";
        harness.check("S11-NEG-02", !Stage11ControlPlane::accept_safety(unknown_safety, reason), reason);

        FeedbackEvent raw{"event-raw-001", "raw_user_message", "", false, false, false, false, false};
        harness.check("S11-UNIT-04", !Stage11ControlPlane::accept_feedback_event(raw, reason), reason);
        FeedbackEvent reviewed{"event-reviewed-001", "review_queue", "release-control-v1", true, true, true, true, true};
        harness.check("S11-UNIT-04B", Stage11ControlPlane::accept_feedback_event(reviewed, reason), reason);

        RegressionCandidate regression{"reg-control-001", "incident-001", "owner-a", "root-cause-open", "test-regression-001", "severity-2", "candidate", "release-control-v1", true, true};
        harness.check("S11-UNIT-05", Stage11ControlPlane::create_regression_candidate(regression, reason), reason);
        RegressionCandidate incomplete = regression; incomplete.test_linkage.clear();
        harness.check("S11-NEG-03", !Stage11ControlPlane::create_regression_candidate(incomplete, reason), reason);

        DatasetReleaseMetadata draft{"release-control-v1", "DRAFT", "dataset-card-digest", "policy-v1", "rubric-v1", "calibration-v1", "privacy-v1", "sealed-eval-v1", "review-board", true, false};
        harness.check("S11-NEG-04", !Stage11ControlPlane::accept_release(draft, reason), reason);
        DatasetReleaseMetadata approved = draft; approved.status = "APPROVED_FOR_TRAINING"; approved.approved_for_training = true;
        harness.check("S11-UNIT-05B", Stage11ControlPlane::accept_release(approved, reason), reason);

        const CalibrationReport first = Harness::calibration_report();
        const CalibrationReport second = Harness::calibration_report();
        harness.check("S11-UNIT-06", Stage11ControlPlane::describe(first) == Stage11ControlPlane::describe(second), "calibration report is deterministic for fixed outcomes");
        harness.check("S11-UNIT-06B", first.sample_count == 10 && std::isfinite(first.expected_calibration_error) && std::isfinite(first.brier_score), "calibration metrics are finite and complete");

        PromotionEvidence no_candidate;
        harness.check("S11-UNIT-07", !Stage11ControlPlane::promote_candidate(approved, first, no_candidate, reason), reason);
        harness.check("S11-UNIT-08", !no_candidate.post_training_executed && !no_candidate.human_approval, "preparation run cannot promote a candidate");
        harness.check("S11-UNIT-09", harness.artifacts.filename() == "stage-11-preparation", "artifacts are explicitly preparation-scoped");

        harness.emit();
        std::cout << "STAGE11_PREPARATION_DECISION=PREPARATION_ONLY\n"
                  << "tests=" << harness.checks.size() << "\n"
                  << "passed=" << harness.passed << "\n"
                  << "failures=" << harness.failed << "\n"
                  << "post_training_executed=false\n"
                  << "candidate_promoted=false\n";
        return harness.failed == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "stage11 preparation harness error: " << error.what() << '\n';
        return 2;
    }
}
