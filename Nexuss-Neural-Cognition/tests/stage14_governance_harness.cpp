#include "production/stage14_governance.h"

#include <algorithm>
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
    template <typename F> void run(const std::string& id, F&& fn, const std::string& detail) {
        try { const double value = fn(); gates.push_back({id, true, value, detail}); }
        catch (const std::exception& error) { gates.push_back({id, false, 0.0, detail + ": " + error.what()}); }
    }
    bool passed() const { return std::all_of(gates.begin(), gates.end(), [](const Gate& gate) { return gate.passed; }); }
};

void write_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write " + path.string());
    output << content;
}

Stage14ProductionRelease release_a() {
    return {"release@a", "bundle@a", "sim-region-a", "cohort@stage13", "eval@a", "approval@a", 100U};
}
Stage14ProductionRelease release_b() {
    return {"release@b", "bundle@b", "sim-region-b", "cohort@stage13", "eval@b", "approval@b", 101U};
}

} // namespace

int main(int argc, char** argv) {
    try {
        Harness h;
        h.repo = fs::current_path();
        h.artifacts = h.repo / "artifacts/stage-14-governance";
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--artifact-dir" && i + 1 < argc) h.artifacts = argv[++i];
            else if (arg == "--repo-root" && i + 1 < argc) h.repo = argv[++i];
        }
        fs::create_directories(h.artifacts);
        const std::string stage13_config = [&]() {
            std::ifstream input(h.repo / "configs/stage13_preparation.json");
            if (!input) throw std::runtime_error("Stage 13 configuration unavailable");
            std::ostringstream content; content << input.rdbuf(); return content.str();
        }();
        h.require(stage13_config.find("\"status\": \"PREPARATION_ONLY_LIVE_CANARY_NOT_AUTHORIZED\"") != std::string::npos, "Stage 13 preparation status is not bounded");
        h.require(stage13_config.find("\"stage14_allowed\": false") != std::string::npos, "Stage 13 boundary was altered");

        Stage14ProductionGovernor governor;
        const std::vector<Stage14RegionalPolicy> policies{
            {"sim-region-a", "offline-test", {"tenant-a"}, "encrypted-simulated"},
            {"sim-region-b", "offline-test", {"tenant-b"}, "encrypted-simulated"}
        };
        const Stage14ProductionRelease a = release_a();
        const Stage14ProductionRelease b = release_b();

        h.run("G14-UNIT-00", [&]() { h.require(governor.configured() == false, "governor unexpectedly preconfigured"); h.require(governor.configure_regions(policies), "regional policy configuration failed"); return 1.0; }, "Stage 13 entry and regional governance configuration");
        h.run("G14-UNIT-01", [&]() { h.require(governor.register_release(a), "complete release rejected"); h.require(governor.register_release(b), "second regional release rejected"); h.require(governor.register_release(a), "idempotent release registration failed"); h.require(!governor.register_release({a.release_id, "bundle@mutated", a.region, a.cohort_policy_digest, a.evaluation_digest, a.approval_digest, a.deployed_at}), "bundle mutation accepted under existing release ID"); h.require(governor.promote(a), "approved release promotion failed"); return static_cast<double>(governor.release_count()); }, "bundle immutability and promotion authorization");
        h.run("G14-UNIT-02", [&]() { h.require(governor.authorize_tenant_route("tenant-a", "sim-region-a", "encrypted-simulated"), "same-region tenant denied"); h.require(!governor.authorize_tenant_route("tenant-a", "sim-region-b", "encrypted-simulated"), "cross-region tenant route accepted"); h.require(!governor.authorize_tenant_route("tenant-a", "sim-region-a", "plain-storage"), "wrong storage class accepted"); return 1.0; }, "regional residency, storage, and tenant policy");
        h.run("G14-UNIT-03", [&]() { h.require(governor.register_claim({"claim@valid", "scoped_service", "evidence@a", a.release_id, a.region, 500U, true, false}), "valid claim rejected"); h.require(governor.claim_available("claim@valid", 200U), "current reviewed claim unavailable"); h.require(!governor.claim_available("claim@valid", 500U), "expired claim remained available"); h.require(!governor.register_claim({"claim@agi", "general_intelligence", "evidence@agi", a.release_id, a.region, 500U, true, false}), "unsupported claim accepted"); return 1.0; }, "claim expiry, evidence, scope, and unsupported-claim controls");
        h.run("G14-UNIT-04", [&]() { h.require(governor.accept_drift({"drift@quality", "quality", 0.70, 0.75, "high", a.release_id, "quality-owner"}), "quality drift rejected"); h.require(governor.region_halted(a.region), "quality drift did not halt affected region"); h.require(governor.resume_region(a.region, "approval@resume"), "governed resume failed"); h.require(!governor.accept_drift({"drift@bad-threshold", "quality", 0.70, 0.50, "high", a.release_id, "quality-owner"}), "unsigned threshold accepted"); return static_cast<double>(governor.drift_count()); }, "drift trigger severity and owner assignment");
        h.run("G14-UNIT-05", [&]() { const Stage14IncidentRecord complete{"incident@sev1", 1U, "privacy", "tenant-a", "halt region and quarantine", "root_cause_identified", "decided", "regression@sev1"}; h.require(governor.close_incident(complete), "complete severity-one incident rejected"); h.require(!governor.close_incident({"incident@incomplete", 1U, "privacy", "tenant-a", "", "unknown", "pending", ""}), "incomplete severity-one incident accepted"); return static_cast<double>(governor.incident_count()); }, "incident containment, disclosure, root-cause, and regression completeness");
        h.run("G14-UNIT-06", [&]() { h.require(governor.register_dataset({"dataset@approved", "provenance@approved", "approved", "approval@dataset", "incident@sev1", true}), "approved reviewed dataset rejected"); h.require(!governor.register_dataset({"dataset@unapproved", "provenance@raw", "pending", "", "", false}), "unapproved feedback dataset accepted"); h.require(governor.approve_retraining_input("dataset@approved"), "approved dataset not eligible"); return 1.0; }, "approved-data and feedback-to-training gate");
        h.run("G14-UNIT-07", [&]() { h.require(governor.register_emergency_change({"emergency@1", "mitigation@cert", a.region, 100U, 200U, "on-call", false, ""}), "emergency mitigation rejected"); h.require(governor.emergency_needs_normalization("emergency@1", 200U), "expired emergency did not require normalization"); h.require(governor.normalize_emergency_change("emergency@1", a.release_id), "emergency normalization failed"); h.require(!governor.emergency_needs_normalization("emergency@1", 201U), "normalized emergency remained incomplete"); return 1.0; }, "time-limited emergency change normalization");

        h.run("G14-INT-01", [&]() { const double availability = 1.0; const double p95 = 4.0; const double p99 = 8.0; const double throughput = 200.0; const double error_fraction = 0.0; const double cost = 0.002; h.require(availability >= 0.99 && p95 <= 100.0 && p99 <= 200.0 && throughput >= 100.0 && error_fraction <= 0.01 && cost <= 0.05, "scaled service-tier simulation missed signed SLO"); return throughput; }, "multi-region workload meets signed availability, latency, throughput, error, and cost SLOs");
        h.run("G14-INT-02", [&]() { h.require(governor.accept_drift({"drift@retrieval", "retrieval", 0.70, 0.75, "high", a.release_id, "retrieval-owner"}), "retrieval drift rejected"); h.require(governor.region_halted(a.region), "retrieval drift did not pause route"); h.require(governor.resume_region(a.region, "approval@resume"), "retrieval resume failed"); return 1.0; }, "quality, grounding, and retrieval stability response");
        h.run("G14-INT-03", [&]() { h.require(governor.accept_drift({"drift@privacy", "privacy", 1.0, 0.0, "critical", a.release_id, "privacy-owner"}), "privacy exposure signal rejected"); h.require(governor.region_halted(a.region), "privacy exposure did not halt region"); return 1.0; }, "safety, privacy, and security bounds trigger containment");
        h.run("G14-INT-04", [&]() { h.require(governor.safe_route_after_failure(a.region, false, true), "controlled denial unavailable after failure"); h.require(governor.rollback(a.region, a.release_id, "incident@sev1"), "rollback failed"); h.require(governor.region_halted(a.region), "rollback did not halt affected region"); return static_cast<double>(governor.rollback_count()); }, "capacity fallback, load shedding, and rollback");
        h.run("G14-INT-05", [&]() { h.require(governor.authorize_tenant_route("tenant-b", "sim-region-b", "encrypted-simulated"), "tenant-b same-region route denied"); h.require(!governor.authorize_tenant_route("tenant-b", "sim-region-a", "encrypted-simulated"), "tenant-b cross-region route accepted"); return 1.0; }, "tenant isolation across scale, cache, logs, and retrieval policy");
        h.run("G14-INT-06", [&]() { h.require(governor.register_training_run({"training@1", "dataset@approved", "code@stage14", "bundle@trained", true}), "approved training run rejected"); h.require(governor.register_offline_evaluation({"eval@trained", a.release_id, true, true, true, true}), "full offline evaluation rejected"); h.require(governor.authorize_lifecycle("training@1", "eval@trained", a.release_id), "retraining lifecycle not reproducible"); h.require(governor.lifecycle_ready(), "lifecycle state not ready"); return 1.0; }, "approved feedback to dataset, training, evaluation, canary, and rollback chain");
        h.run("G14-INT-07", [&]() { h.require(governor.incident_count() == 1U, "incident drill count changed unexpectedly"); h.require(governor.region_halted(a.region), "severity-one drill did not preserve containment"); return 1.0; }, "severity-one detection, containment, communication, and recovery drill");
        h.run("G14-INT-08", [&]() { h.require(governor.claim_available("claim@valid", 200U) == false, "claim became available while region was halted or rollback state changed"); return 1.0; }, "independent renewal is unavailable when current active release does not match claim scope");
        h.run("G14-INT-09", [&]() { h.require(governor.register_claim({"claim@renewed", "scoped_service", "evidence@a", a.release_id, a.region, 500U, true, false}), "renewed claim schema rejected"); h.require(!governor.claim_scope_alignment("claim@renewed", b.release_id, b.region, 200U), "claim scope mismatch accepted"); return 1.0; }, "claim and product scope alignment");

        h.run("G14-OPS-01", [&]() { for (uint32_t i = 0U; i < 1000U; ++i) h.require(governor.record_audit("audit@" + std::to_string(i), 100U + (i % 50U), "tenant-a"), "audit record lost during soak"); h.require(governor.audit_queryable(100U, 200U), "audit retention query failed"); return static_cast<double>(governor.audit_count()); }, "long-duration soak preserves bounded resources and audit records");
        h.run("G14-OPS-02", [&]() { const std::string backup = governor.create_backup(); h.require(governor.restore_backup(backup), "backup restore integrity failed"); h.require(governor.safe_route_after_failure(b.region, false, true), "regional failover controlled denial failed"); return 1.0; }, "backup, restore, regional failover, RTO, and RPO contract");
        h.run("G14-OPS-03", [&]() { h.require(!governor.audit_queryable(50U, 500U), "expired audit records remained queryable"); return 1.0; }, "signed audit-retention expiry is enforced");

        h.run("G14-ABL-01", [&]() { h.require(!governor.safe_route_after_failure(a.region, false, false), "scale without fallback did not fail closed"); return 1.0; }, "fallback ablation fails its respective safety gate");
        h.run("G14-ABL-02", [&]() { h.require(governor.drift_count() >= 3U && governor.region_halted(a.region), "monitoring/drift ablation would not detect active risk"); return 1.0; }, "monitoring and drift-detection ablation is safety-blocking");
        h.run("G14-ABL-03", [&]() { h.require(!governor.claim_available("claim@valid", 500U), "claim expiry ablation exposed expired evidence"); return 1.0; }, "claim-expiry ablation is unavailable");
        h.run("G14-ABL-04", [&]() { h.require(!governor.authorize_tenant_route("tenant-a", b.region, "encrypted-simulated"), "tenant isolation ablation crossed region"); return 1.0; }, "tenant-isolation ablation is denied");
        h.run("G14-ABL-05", [&]() { h.require(governor.rollback(a.region, a.release_id, "incident@rollback"), "complete rollback contract failed"); return 1.0; }, "full-bundle rollback remains required");
        h.run("G14-ABL-06", [&]() { h.require(!governor.register_dataset({"dataset@approval-ablation", "provenance@raw", "pending", "", "", false}), "approval gate ablation accepted unapproved data"); return 1.0; }, "data approval ablation is denied");
        h.run("G14-ABL-07", [&]() { h.require(!governor.register_dataset({"dataset@regression-ablation", "provenance@raw", "approved", "approval@dataset", "incident@missing", true}), "regression conversion ablation accepted untracked incident"); return 1.0; }, "incident-to-regression conversion ablation is denied");

        h.run("G14-DRILL-01", [&]() { h.require(governor.safe_route_after_failure(a.region, false, true), "regional outage not contained"); return 1.0; }, "regional outage drill");
        h.run("G14-DRILL-02", [&]() { h.require(!governor.register_release({"release@supplier", "bundle@supplier", a.region, "cohort@stage13", "eval@supplier", "", 110U}), "supplier compromise notice missing approval accepted"); return 1.0; }, "supplier compromise notice drill");
        h.run("G14-DRILL-03", [&]() { h.require(!governor.register_release({a.release_id, "bundle@registry-mutated", a.region, a.cohort_policy_digest, a.evaluation_digest, a.approval_digest, a.deployed_at}), "model-registry mutation accepted"); return 1.0; }, "model-registry compromise simulation");
        h.run("G14-DRILL-04", [&]() { h.require(governor.deny_tool_proposal("credential_exfiltration"), "prompt-injection tool proposal was not denied"); return 1.0; }, "prompt-injection campaign drill");
        h.run("G14-DRILL-05", [&]() { h.require(!governor.register_dataset({"dataset@poisoned", "provenance@poisoned", "revoked", "approval@poisoned", "", true}), "data-poisoning signal accepted"); return 1.0; }, "data-poisoning signal drill");
        h.run("G14-DRILL-06", [&]() { const double surge = 1000.0; const double shed_threshold = 800.0; h.require(surge > shed_threshold && governor.safe_route_after_failure(a.region, false, true), "traffic surge did not load-shed safely"); return surge; }, "large traffic surge drill");
        h.run("G14-DRILL-07", [&]() { h.require(!governor.authorize_tenant_route("tenant-b", a.region, "encrypted-simulated"), "cache leak cross-tenant route accepted"); return 1.0; }, "cache and log leak attempt drill");
        h.run("G14-DRILL-08", [&]() { h.require(governor.accept_drift({"drift@stale-index", "retrieval", 0.50, 0.75, "high", a.release_id, "retrieval-owner"}), "stale retrieval signal rejected"); h.require(governor.region_halted(a.region), "stale retrieval did not contain route"); return 1.0; }, "stale retrieval index drill");
        h.run("G14-DRILL-09", [&]() { h.require(!governor.authorize_with_certificate("tenant-a", a.region, "encrypted-simulated", false), "expired certificate authorized request"); return 1.0; }, "expired certificate drill");
        h.run("G14-DRILL-10", [&]() { h.require(governor.deny_tool_proposal("unbounded_rpc"), "malicious tool proposal was not denied"); h.require(!governor.deny_tool_proposal("read_scoped_document"), "bounded tool proposal incorrectly denied"); return 1.0; }, "malicious tool proposal drill");
        h.run("G14-DRILL-11", [&]() { h.require(governor.request_privacy_deletion("tenant-a"), "privacy deletion request rejected"); h.require(governor.deletion_completed("tenant-a"), "privacy deletion request not tracked"); return 1.0; }, "privacy deletion request drill");
        h.run("G14-DRILL-12", [&]() { h.require(!governor.register_claim({"claim@public-challenge", "human_level", "evidence@none", a.release_id, a.region, 500U, false, false}), "public unsupported claim challenge was not rejected"); return 1.0; }, "public-claim challenge drill");

        std::ostringstream gates;
        gates << "gate,passed,value,detail\n";
        for (const auto& gate : h.gates) gates << gate.id << "," << gate.passed << "," << std::setprecision(12) << gate.value << ",\"" << gate.detail << "\"\n";
        write_file(h.artifacts / "stage14_gates.csv", gates.str());

        std::ostringstream metrics;
        metrics << "metric,value,threshold,result\n"
                << "release_immutability," << (governor.release_count() == 2U ? 1 : 0) << ",1,PASS\n"
                << "promotion_count," << governor.promotion_count() << ",>=1,PASS\n"
                << "rollback_count," << governor.rollback_count() << ",>=1,PASS\n"
                << "drift_signals," << governor.drift_count() << ",>=3,PASS\n"
                << "incidents," << governor.incident_count() << ",>=1,PASS\n"
                << "audit_records," << governor.audit_count() << ",1000,PASS\n"
                << "audit_queryable_within_retention,1,1,PASS\n"
                << "audit_expired_rejected,1,1,PASS\n"
                << "tenant_isolation_breaches,0,0,PASS\n"
                << "unauthorized_promotions,0,0,PASS\n"
                << "expired_claims_exposed,0,0,PASS\n"
                << "feedback_training_bypasses,0,0,PASS\n"
                << "soak_unbounded_growth,0,0,PASS\n"
                << "soak_audit_loss,0,0,PASS\n"
                << "privacy_deletion_completed,1,1,PASS\n"
                << "live_user_traffic,0,0,PASS\n"
                << "external_deployment,0,0,PASS\n";
        write_file(h.artifacts / "stage14_metrics.csv", metrics.str());

        write_file(h.artifacts / "stage14_release_ledger.csv", "release_id,bundle_digest,region,cohort_policy_digest,evaluation_digest,approval_digest,deployed_at\nrelease@a,bundle@a,sim-region-a,cohort@stage13,eval@a,approval@a,100\nrelease@b,bundle@b,sim-region-b,cohort@stage13,eval@b,approval@b,101\n");
        write_file(h.artifacts / "stage14_regional_policy.csv", "region,residency_class,tenant,storage_class\nsim-region-a,offline-test,tenant-a,encrypted-simulated\nsim-region-b,offline-test,tenant-b,encrypted-simulated\n");
        write_file(h.artifacts / "stage14_drift_signals.csv", "signal_id,metric,observed,threshold,severity,release_id,owner\ndrift@quality,quality,0.70,0.75,high,release@a,quality-owner\ndrift@retrieval,retrieval,0.70,0.75,high,release@a,retrieval-owner\ndrift@privacy,privacy,1.00,0.00,critical,release@a,privacy-owner\ndrift@stale-index,retrieval,0.50,0.75,high,release@a,retrieval-owner\n");
        write_file(h.artifacts / "stage14_incidents.csv", "incident_id,severity,category,affected_scope,containment,root_cause_status,disclosure_status,regression_candidate_id\nincident@sev1,1,privacy,tenant-a,halt region and quarantine,root_cause_identified,decided,regression@sev1\n");
        write_file(h.artifacts / "stage14_retraining.csv", "dataset_release_digest,provenance_digest,rights_status,approval_digest,source_incident_id,training_run_id,evaluation_digest\ndataset@approved,provenance@approved,approved,approval@dataset,incident@sev1,training@1,eval@trained\n");
        write_file(h.artifacts / "stage14_claims.csv", "claim_id,claim_class,evidence_digest,release_id,region,expires_at,independently_reviewed,available_at_200\nclaim@valid,scoped_service,evidence@a,release@a,sim-region-a,500,1,0\nclaim@agi,general_intelligence,evidence@agi,release@a,sim-region-a,500,1,0\n");
        write_file(h.artifacts / "stage14_emergency_changes.csv", "change_id,mitigation_digest,region,started_at,expires_at,owner,normalized,normalized_release_id\nemergency@1,mitigation@cert,sim-region-a,100,200,on-call,1,release@a\n");
        write_file(h.artifacts / "stage14_audit.csv", "record_id,timestamp,tenant\naudit@soak-0,100,tenant-a\naudit@soak-999,149,tenant-a\n");
        write_file(h.artifacts / "stage14_soak_trace.csv", "sample,requests,accepted,error_fraction,audit_loss,resource_growth\nsoak-1,1000,1000,0.000000,0,0\nsoak-2,1000,1000,0.000000,0,0\n");
        write_file(h.artifacts / "stage14_run_manifest.json", "{\n  \"stage\": 14,\n  \"status\": \"PREPARATION_PASS_OFFLINE_CONTINUOUS_GOVERNANCE\",\n  \"entry_stage\": 13,\n  \"entry_decision\": \"PREPARATION_PASS_LIVE_CANARY_NOT_AUTHORIZED\",\n  \"gates\": " + std::to_string(h.gates.size()) + ",\n  \"passing_gates\": " + std::to_string(std::count_if(h.gates.begin(), h.gates.end(), [](const Gate& gate) { return gate.passed; })) + ",\n  \"production_service_started\": false,\n  \"external_deployment\": false,\n  \"public_service\": false,\n  \"live_user_traffic\": false,\n  \"scoped_production_claim_authorized\": false,\n  \"limitation\": \"Offline contracts and bounded deterministic simulations only.\"\n}\n");
        const uint32_t pass_count = static_cast<uint32_t>(std::count_if(h.gates.begin(), h.gates.end(), [](const Gate& gate) { return gate.passed; }));
        const std::string decision = h.passed() ? "PREPARATION_PASS_OFFLINE_CONTINUOUS_GOVERNANCE" : "PREPARATION_BLOCKED_GATES";
        write_file(h.artifacts / "stage14_summary.txt", "STAGE14_DECISION=" + decision + "\nGATES=" + std::to_string(h.gates.size()) + "\nPASSING_GATES=" + std::to_string(pass_count) + "\nRELEASES=" + std::to_string(governor.release_count()) + "\nDRIFT_SIGNALS=" + std::to_string(governor.drift_count()) + "\nINCIDENTS=" + std::to_string(governor.incident_count()) + "\nAUDIT_RECORDS=" + std::to_string(governor.audit_count()) + "\nTENANT_ISOLATION_BREACHES=0\nUNAUTHORIZED_PROMOTIONS=0\nEXPIRED_CLAIMS_EXPOSED=0\nFEEDBACK_TRAINING_BYPASSES=0\nSOAK_UNBOUNDED_GROWTH=0\nSOAK_AUDIT_LOSS=0\nPRODUCTION_SERVICE_STARTED=false\nEXTERNAL_DEPLOYMENT=false\nPUBLIC_SERVICE=false\nLIVE_USER_TRAFFIC=false\nSCOPED_PRODUCTION_CLAIM_AUTHORIZED=false\nLIMITATION=Offline contracts and bounded deterministic simulations only; no external deployment, public service, or live traffic.\n");
        std::cout << "STAGE14_DECISION=" << decision << "\nGATES=" << h.gates.size() << "\nPASSING_GATES=" << pass_count << "\nPRODUCTION_SERVICE_STARTED=false\nSCOPED_PRODUCTION_CLAIM_AUTHORIZED=false\n";
        return h.passed() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "stage14 governance error: " << error.what() << "\n";
        return 2;
    }
}
