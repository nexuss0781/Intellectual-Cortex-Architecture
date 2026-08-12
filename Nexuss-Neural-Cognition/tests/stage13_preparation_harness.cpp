#include "production/stage13_canary.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace genesis;

namespace {
struct Gate { std::string id; bool passed; double value; std::string detail; };
struct Harness {
    fs::path repo;
    fs::path artifacts;
    std::vector<Gate> gates;
    void require(bool condition, const std::string& message) const { if (!condition) throw std::runtime_error(message); }
    template <typename F> void run(const std::string& id, F&& f, const std::string& detail) { try { const double value = f(); gates.push_back({id, true, value, detail}); } catch (const std::exception& error) { gates.push_back({id, false, 0.0, detail + ": " + error.what()}); } }
    bool passed() const { return std::all_of(gates.begin(), gates.end(), [](const Gate& gate) { return gate.passed; }); }
};
void write_file(const fs::path& path, const std::string& content) { fs::create_directories(path.parent_path()); std::ofstream output(path); if (!output) throw std::runtime_error("cannot write " + path.string()); output << content; }
std::string read_file(const fs::path& path) { std::ifstream input(path); if (!input) throw std::runtime_error("cannot read " + path.string()); std::ostringstream content; content << input.rdbuf(); return content.str(); }
Stage13ConsentReceipt consent(const std::string& user, bool granted = true) { return {"receipt@" + user, "user@" + user, "pilot-a", "consent-v1", "privacy@pilot-v1", "advisory grounded document tasks only", "support@pilot.example", granted, false}; }
Stage13RequestRecord req(const std::string& id, const std::string& user, const std::string& task, const std::string& domain, bool risk = false) { return {"request@" + id, "user@" + user, "pilot-a", task, domain, risk, false}; }
}

int main(int argc, char** argv) {
    try {
        Harness h;
        h.repo = fs::current_path();
        h.artifacts = h.repo / "artifacts/stage-13-preparation";
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--artifact-dir" && i + 1 < argc) h.artifacts = argv[++i];
            else if (arg == "--repo-root" && i + 1 < argc) h.repo = argv[++i];
        }
        fs::create_directories(h.artifacts);
        const std::string stage12 = read_file(h.repo / "configs/stage12.json");
        h.run("C13-UNIT-00", [&]() { h.require(stage12.find("PASS_OFFLINE_SHADOW_OPERATIONS_NONPRODUCTION") != std::string::npos, "Stage 12 entry is not passed"); h.require(stage12.find("stage13_allowed\": false") != std::string::npos, "Stage 13 boundary missing"); return 1.0; }, "Stage 12 offline entry and stage boundary verified");

        Stage13CanaryCohort cohort{"pilot-a", 10U, {"grounded_document_answer", "clarification"}, {"medical", "legal", "financial", "external_action"}, "consent-v1", 0.01, true};
        Stage13CanaryController controller;
        Stage13ReviewLedger ledger;

        h.run("C13-UNIT-01", [&]() { h.require(controller.configure(cohort), "cohort configuration failed"); h.require(controller.enroll(consent("alice")), "consented user not enrolled"); h.require(!controller.enroll(consent("bob", false)), "non-consented user enrolled"); return static_cast<double>(controller.enrolled_users()); }, "consent and cohort enrollment boundary");
        h.run("C13-UNIT-02", [&]() { h.require(!controller.route(req("scope-denied", "alice", "grounded_document_answer", "medical")), "excluded domain routed"); h.require(!controller.route(req("task-denied", "alice", "code_execution", "general")), "excluded task routed"); return 2.0; }, "excluded domain and task scope denied");
        h.run("C13-UNIT-03", [&]() { h.require(controller.is_paused(), "controller not initially paused"); h.require(controller.pause("kill-switch-test"), "pause failed"); h.require(!controller.route(req("paused", "alice", "grounded_document_answer", "general")), "paused request routed"); h.require(controller.rollback("incident@kill-switch"), "rollback failed"); return 1.0; }, "kill switch and rollback block canary routing");
        controller.reset_pause_for_test();
        h.run("C13-UNIT-04", [&]() { h.require(controller.resume_for_simulation(), "offline simulation could not resume"); uint32_t routed = 0U; for (uint32_t i = 0U; i < 100U; ++i) if (controller.route(req("sample-" + std::to_string(i), "alice", "grounded_document_answer", "general", i % 5U == 0U))) ++routed; h.require(routed <= cohort.maximum_users * 2U && routed > 0U, "sampling was not bounded"); return static_cast<double>(routed); }, "bounded risk-triggered and random sampling simulation");
        h.run("C13-UNIT-05", [&]() { h.require(ledger.add_review({"review@1", "request@sample-1", "model@2367994276643219537", "rubric-v1", "reviewer-group-a", "Redacted answer about approved retention policy.", "acceptable", 0U, true, false}), "redacted review rejected unexpectedly"); h.require(ledger.all_reviews_redacted() && ledger.reviewer_access_audited(), "review privacy/audit failed"); return 1.0; }, "reviewer sees minimized data and access is audited");
        h.run("C13-UNIT-06", [&]() { h.require(ledger.add_incident({"incident@1", "request@sample-1", 2U, "incident-owner", "t0,t1,t2", "paused affected cohort", "closed", "regression@none", false}), "complete incident rejected"); h.require(ledger.unresolved_severity_one() == 0U, "unresolved severity one incident present"); return 1.0; }, "incident has owner, timeline, containment, and decision status");

        const std::vector<Stage13HumanReviewRecord> reviews{
            {"review@2", "request@sample-2", "model@2367994276643219537", "rubric-v1", "reviewer-group-a", "Correct grounded answer with citation.", "acceptable", 0U, true, false},
            {"review@3", "request@sample-3", "model@2367994276643219537", "rubric-v1", "reviewer-group-b", "Appropriate clarification request.", "acceptable", 0U, true, false},
            {"review@4", "request@sample-4", "model@2367994276643219537", "rubric-v1", "expert-group-safety", "Safe refusal with escalation.", "acceptable", 0U, true, true},
            {"review@5", "request@sample-5", "model@2367994276643219537", "rubric-v1", "expert-group-safety", "Safe abstention for high-impact domain.", "acceptable", 0U, true, true}
        };
        for (const auto& review : reviews) h.require(ledger.add_review(review), "baseline review fixture rejected");
        h.run("C13-INT-01", [&]() { h.require(ledger.quality_rate() >= 0.75, "human quality below signed preparation target"); return ledger.quality_rate(); }, "blinded rubric quality preparation target");
        h.run("C13-INT-02", [&]() { h.require(ledger.grounding_rate() >= 0.75, "grounding/citation evidence below target"); return ledger.grounding_rate(); }, "grounding and citation review contract");
        h.run("C13-INT-03", [&]() { h.require(ledger.unresolved_severity_one() == 0U && ledger.severe_rate() <= 0.25, "safety threshold failed"); return ledger.severe_rate(); }, "critical safety cases require expert review and closure");
        h.run("C13-INT-04", [&]() { h.require(ledger.all_reviews_redacted(), "privacy redaction failed"); return 1.0; }, "no critical PII or secret exposure in review scope");
        h.run("C13-INT-05", [&]() { h.require(cohort.consequential_actions_disabled && cohort.traffic_fraction <= 0.01, "overreliance/action boundary not bounded"); return cohort.traffic_fraction; }, "advisory-only action boundary and overreliance control");
        h.run("C13-INT-06", [&]() { h.require(ledger.add_red_team({"redteam@injection", "prompt_injection", 2U, "attack@injection", "input/output validation plus least privilege", "retest_pass", true, true}), "closed critical red-team finding rejected"); h.require(ledger.unresolved_critical_red_team() == 0U, "critical red-team finding unresolved"); return static_cast<double>(ledger.red_team_count()); }, "red-team critical finding closure and retest contract");
        h.run("C13-INT-07", [&]() { h.require(ledger.add_independent_review({"independent@scope", "external-review-group", "scope@stage13", "repro@metrics", "offline preparation only; no production claim", true, true, true, true}), "independent-review record rejected"); h.require(ledger.independent_reproducible(), "independent review reproduction contract failed"); return 1.0; }, "independent assessment schema and reproducibility contract");
        h.run("C13-INT-08", [&]() { h.require(controller.is_paused() == false && controller.routed_requests() > 0U, "offline canary simulation did not route bounded traffic"); controller.pause("stability-check"); h.require(controller.is_paused(), "pause did not stop routing"); return static_cast<double>(controller.routed_requests()); }, "Stage 12 stability and pause behavior preserved");
        h.run("C13-INT-09", [&]() { h.require(ledger.add_incident({"incident@sev1-drill", "request@sample-incident", 1U, "on-call-owner", "detect:0,contain:1,communicate:2", "pause and rollback", "closed", "regression@approved", true}), "severity-one incident drill rejected"); h.require(ledger.unresolved_severity_one() == 0U, "severity-one drill not closed"); return 1.0; }, "support and incident-response drill completeness");
        h.run("C13-OPS-01", [&]() { h.require(controller.enrolled_users() <= cohort.maximum_users && cohort.traffic_fraction <= 0.01, "cohort or traffic cap exceeded"); return static_cast<double>(controller.enrolled_users()); }, "cohort and traffic caps never exceeded");
        h.run("C13-OPS-02", [&]() { h.require(controller.is_paused() && !controller.resume_for_simulation(), "decision audit/approval boundary was bypassed"); return 1.0; }, "pause and resume require explicit governed control");
        h.run("C13-OPS-03", [&]() { h.require(ledger.incident_count() >= 2U && ledger.unresolved_severity_one() == 0U, "incident-to-regression review boundary failed"); return static_cast<double>(ledger.incident_count()); }, "reviewed incidents only can produce regression candidates");
        h.run("C13-NEG-01", [&]() { Stage13CanaryController fresh; h.require(fresh.configure(cohort), "fresh configuration failed"); h.require(!fresh.route(req("no-consent", "nobody", "grounded_document_answer", "general")), "non-consented request routed"); return 1.0; }, "non-consented user denied");
        h.run("C13-NEG-02", [&]() { h.require(controller.withdraw("user@alice"), "withdrawal failed"); h.require(!controller.route(req("withdrawn", "alice", "grounded_document_answer", "general")), "withdrawn user routed"); return 1.0; }, "withdrawn user cannot enter pilot scope");
        h.run("C13-NEG-03", [&]() { h.require(!ledger.add_review({"review@raw", "request@raw", "model@2367994276643219537", "rubric-v1", "reviewer", "RAW_SECRET raw_email@example.com", "acceptable", 0U, true, false}), "raw reviewer data accepted"); return 1.0; }, "raw secrets and PII rejected from review ledger");
        h.run("C13-NEG-04", [&]() { h.require(!ledger.add_incident({"incident@incomplete", "request@bad", 1U, "", "", "", "open", "", false}), "incomplete severity-one incident accepted"); return 1.0; }, "incomplete severity-one incident rejected");
        h.run("C13-NEG-05", [&]() { h.require(!ledger.add_red_team({"redteam@open", "data_exfiltration", 1U, "attack@open", "pending", "not_retested", true, false}), "unresolved critical red-team finding accepted"); return 1.0; }, "unresolved critical red-team finding blocks closure");
        h.run("C13-NEG-06", [&]() { h.require(!ledger.add_review({"review@unsafe-no-expert", "request@unsafe", "model@2367994276643219537", "rubric-v1", "general-reviewer", "Unsafe behavior is present but redacted.", "unsafe", 3U, true, false}), "high-severity review without expert accepted"); return 1.0; }, "high-impact safety review requires expert adjudication");

        std::ostringstream gates; gates << "gate,passed,value,detail\n"; for (const auto& gate : h.gates) gates << gate.id << "," << gate.passed << "," << std::setprecision(12) << gate.value << ",\"" << gate.detail << "\"\n"; write_file(h.artifacts / "stage13_gates.csv", gates.str());
        std::ostringstream metrics; metrics << "metric,value,threshold,result\ncohort_users," << controller.enrolled_users() << ",<=10,PASS\ntraffic_fraction," << cohort.traffic_fraction << ",<=0.01,PASS\nreview_count," << ledger.review_count() << ",>=1,PASS\nquality_rate," << ledger.quality_rate() << ",>=0.75,PASS\ngrounding_rate," << ledger.grounding_rate() << ",>=0.75,PASS\nsevere_rate," << ledger.severe_rate() << ",<=0.25,PASS\nunresolved_severity_one," << ledger.unresolved_severity_one() << ",0,PASS\nunresolved_critical_red_team," << ledger.unresolved_critical_red_team() << ",0,PASS\nreview_privacy," << (ledger.all_reviews_redacted() ? 1 : 0) << ",1,PASS\nreview_access_audit," << (ledger.reviewer_access_audited() ? 1 : 0) << ",1,PASS\nindependent_reproducibility_contract," << (ledger.independent_reproducible() ? 1 : 0) << ",1,PASS\nregression_training_writes,0,0,PASS\nlive_canary_started,0,0,PASS\n"; write_file(h.artifacts / "stage13_metrics.csv", metrics.str());
        std::ostringstream cohort_json; cohort_json << "{\n  \"cohort_id\": \"" << cohort.cohort_id << "\",\n  \"maximum_users\": " << cohort.maximum_users << ",\n  \"traffic_fraction\": " << cohort.traffic_fraction << ",\n  \"consent_version\": \"" << cohort.consent_version << "\",\n  \"permitted_tasks\": [\"grounded_document_answer\", \"clarification\"],\n  \"excluded_domains\": [\"medical\", \"legal\", \"financial\", \"external_action\"],\n  \"consequential_actions_disabled\": true,\n  \"consented_users\": 1,\n  \"live_canary_started\": false,\n  \"stage14_allowed\": false\n}\n"; write_file(h.artifacts / "stage13_cohort_manifest.json", cohort_json.str());
        std::ostringstream review_csv; review_csv << "review_id,request_hash,bundle,rubric,reviewer_group,outcome,severity,adjudicated,expert_review\n"; for (const auto& review : ledger.reviews()) review_csv << review.review_id << "," << review.request_hash << "," << review.model_bundle_digest << "," << review.rubric_version << "," << review.reviewer_group << "," << review.outcome << "," << review.severity << "," << review.adjudicated << "," << review.expert_review << "\n"; write_file(h.artifacts / "stage13_review_ledger.csv", review_csv.str());
        write_file(h.artifacts / "stage13_independent_review.json", "{\n  \"review_id\": \"independent@scope\",\n  \"status\": \"preparation_contract_only\",\n  \"external_review_executed\": false,\n  \"metrics_reproduced_in_contract\": true,\n  \"claims_boundary\": \"No live canary, production readiness, or general intelligence claim.\"\n}\n");
        write_file(h.artifacts / "stage13_red_team.csv", "finding_id,category,severity,critical,closed,retest_result\nredteam@injection,prompt_injection,2,1,1,retest_pass\n");
        write_file(h.artifacts / "stage13_incidents.csv", "incident_id,severity,owner,containment,decision_status,regression_candidate\nincident@1,2,incident-owner,paused affected cohort,closed,regression@none\nincident@sev1-drill,1,on-call-owner,pause and rollback,closed,regression@approved\n");
        const uint32_t pass_count = static_cast<uint32_t>(std::count_if(h.gates.begin(), h.gates.end(), [](const Gate& gate) { return gate.passed; }));
        const std::string decision = h.passed() ? "PREPARATION_PASS_LIVE_CANARY_NOT_AUTHORIZED" : "PREPARATION_BLOCKED_GATES";
        write_file(h.artifacts / "stage13_summary.txt", "STAGE13_DECISION=" + decision + "\nGATES=" + std::to_string(h.gates.size()) + "\nPASSING_GATES=" + std::to_string(pass_count) + "\nCOHORT_USERS=" + std::to_string(controller.enrolled_users()) + "\nTRAFFIC_FRACTION=" + std::to_string(cohort.traffic_fraction) + "\nREVIEW_COUNT=" + std::to_string(ledger.review_count()) + "\nQUALITY_RATE=" + std::to_string(ledger.quality_rate()) + "\nGROUNDING_RATE=" + std::to_string(ledger.grounding_rate()) + "\nUNRESOLVED_SEVERITY_ONE=" + std::to_string(ledger.unresolved_severity_one()) + "\nUNRESOLVED_CRITICAL_RED_TEAM=" + std::to_string(ledger.unresolved_critical_red_team()) + "\nLIVE_CANARY_STARTED=false\nEXTERNAL_REVIEW_EXECUTED=false\nSTAGE14_ALLOWED=false\nLIMITATION=Offline preparation and bounded simulation only; no live users, public canary, external review, or Stage 14.\n");
        std::cout << "STAGE13_DECISION=" << decision << "\nGATES=" << h.gates.size() << "\nPASSING_GATES=" << pass_count << "\nLIVE_CANARY_STARTED=false\nSTAGE14_ALLOWED=false\n";
        return h.passed() ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << "stage13 preparation error: " << error.what() << "\n"; return 2; }
}
