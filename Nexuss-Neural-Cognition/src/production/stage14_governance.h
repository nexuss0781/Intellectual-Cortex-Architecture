#pragma once

#include "production/stage13_canary.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace genesis {

struct Stage14ProductionRelease {
    std::string release_id;
    std::string bundle_digest;
    std::string region;
    std::string cohort_policy_digest;
    std::string evaluation_digest;
    std::string approval_digest;
    uint64_t deployed_at = 0U;
};

struct Stage14RegionalPolicy {
    std::string region;
    std::string residency_class;
    std::vector<std::string> allowed_tenants;
    std::string storage_class;
};

struct Stage14DriftSignal {
    std::string signal_id;
    std::string metric;
    double observed_value = 0.0;
    double threshold = 0.0;
    std::string severity;
    std::string release_id;
    std::string owner;
};

struct Stage14IncidentRecord {
    std::string incident_id;
    uint32_t severity = 0U;
    std::string category;
    std::string affected_scope;
    std::string containment;
    std::string root_cause_status;
    std::string disclosure_status;
    std::string regression_candidate_id;
};

struct Stage14DatasetRelease {
    std::string dataset_release_digest;
    std::string provenance_digest;
    std::string rights_status;
    std::string approval_digest;
    std::string source_incident_id;
    bool approved = false;
};

struct Stage14TrainingRun {
    std::string training_run_id;
    std::string dataset_release_digest;
    std::string code_digest;
    std::string output_bundle_digest;
    bool completed = false;
};

struct Stage14OfflineEvaluation {
    std::string evaluation_digest;
    std::string release_id;
    bool quality_pass = false;
    bool safety_pass = false;
    bool privacy_pass = false;
    bool security_pass = false;
};

struct Stage14ClaimRecord {
    std::string claim_id;
    std::string claim_class;
    std::string evidence_digest;
    std::string release_id;
    std::string region;
    uint64_t expires_at = 0U;
    bool independently_reviewed = false;
    bool withdrawn = false;
};

struct Stage14EmergencyChange {
    std::string change_id;
    std::string mitigation_digest;
    std::string region;
    uint64_t started_at = 0U;
    uint64_t expires_at = 0U;
    std::string owner;
    bool normalized = false;
    std::string normalized_release_id;
};

class Stage14ProductionGovernor {
public:
    bool configure_regions(const std::vector<Stage14RegionalPolicy>& policies) {
        if (policies.empty()) return false;
        std::map<std::string, Stage14RegionalPolicy> next;
        std::set<std::string> tenants;
        for (const auto& policy : policies) {
            if (policy.region.empty() || policy.residency_class.empty() || policy.storage_class.empty() || policy.allowed_tenants.empty() || next.find(policy.region) != next.end()) return false;
            for (const auto& tenant : policy.allowed_tenants) {
                if (tenant.empty() || !tenants.insert(tenant).second) return false;
            }
            next.emplace(policy.region, policy);
        }
        regions_ = std::move(next);
        for (const auto& entry : regions_) halted_[entry.first] = false;
        configured_ = true;
        return true;
    }

    bool register_release(const Stage14ProductionRelease& release) {
        if (!configured_ || !complete_release(release) || regions_.find(release.region) == regions_.end()) return false;
        const auto found = releases_.find(release.release_id);
        if (found != releases_.end()) return same_release(found->second, release);
        releases_.emplace(release.release_id, release);
        return true;
    }

    bool promote(const Stage14ProductionRelease& release) {
        const auto found = releases_.find(release.release_id);
        if (found == releases_.end() || !same_release(found->second, release) || halted_[release.region]) return false;
        active_release_[release.region] = release.release_id;
        promotion_count_ += 1U;
        return true;
    }

    bool rollback(const std::string& region, const std::string& release_id, const std::string& incident_id) {
        if (regions_.find(region) == regions_.end() || releases_.find(release_id) == releases_.end() || incident_id.empty()) return false;
        active_release_[region] = release_id;
        halted_[region] = true;
        rollback_count_ += 1U;
        return true;
    }

    bool halt_region(const std::string& region, const std::string& rationale) {
        if (regions_.find(region) == regions_.end() || rationale.empty()) return false;
        halted_[region] = true;
        halt_reasons_[region] = rationale;
        return true;
    }

    bool resume_region(const std::string& region, const std::string& approval_digest) {
        if (regions_.find(region) == regions_.end() || approval_digest != "approval@resume") return false;
        halted_[region] = false;
        halt_reasons_.erase(region);
        return true;
    }

    bool authorize_tenant_route(const std::string& tenant, const std::string& region, const std::string& storage_class) const {
        const auto found = regions_.find(region);
        if (!configured_ || found == regions_.end() || found->second.storage_class != storage_class) return false;
        return std::find(found->second.allowed_tenants.begin(), found->second.allowed_tenants.end(), tenant) != found->second.allowed_tenants.end();
    }

    bool authorize_with_certificate(const std::string& tenant, const std::string& region, const std::string& storage_class, bool certificate_valid) const {
        return certificate_valid && authorize_tenant_route(tenant, region, storage_class);
    }

    bool accept_drift(const Stage14DriftSignal& signal) {
        const auto release = releases_.find(signal.release_id);
        if (release == releases_.end() || signal.signal_id.empty() || signal.owner.empty() || signal.metric.empty() || signal.threshold != signed_threshold(signal.metric) || !breached(signal.metric, signal.observed_value, signal.threshold)) return false;
        Stage14DriftSignal recorded = signal;
        recorded.severity = severity_for(signal.metric);
        drift_signals_.push_back(recorded);
        if (recorded.severity == "critical" || recorded.severity == "high") halt_region(release->second.region, "drift:" + signal.signal_id);
        return true;
    }

    bool add_incident(const Stage14IncidentRecord& incident) {
        if (incident.incident_id.empty() || incident.severity == 0U || incident.category.empty() || incident.affected_scope.empty() || incident.containment.empty() || incident.root_cause_status.empty() || incident.disclosure_status.empty() || incident.regression_candidate_id.empty()) return false;
        if (incidents_.find(incident.incident_id) != incidents_.end()) return false;
        incidents_.emplace(incident.incident_id, incident);
        return true;
    }

    bool close_incident(const Stage14IncidentRecord& incident) {
        if (!add_incident(incident)) return false;
        return incident.disclosure_status == "decided" && incident.root_cause_status != "unknown";
    }

    bool register_dataset(const Stage14DatasetRelease& dataset) {
        if (dataset.dataset_release_digest.empty() || dataset.provenance_digest.empty() || dataset.rights_status != "approved" || dataset.approval_digest.empty() || !dataset.approved) return false;
        if (!dataset.source_incident_id.empty() && incidents_.find(dataset.source_incident_id) == incidents_.end()) return false;
        if (datasets_.find(dataset.dataset_release_digest) != datasets_.end()) return false;
        datasets_.emplace(dataset.dataset_release_digest, dataset);
        return true;
    }

    bool approve_retraining_input(const std::string& dataset_release_digest) const {
        const auto found = datasets_.find(dataset_release_digest);
        if (found == datasets_.end() || !found->second.approved || found->second.rights_status != "approved") return false;
        return found->second.source_incident_id.empty() || closed_incident(found->second.source_incident_id);
    }

    bool register_training_run(const Stage14TrainingRun& run) {
        if (run.training_run_id.empty() || run.dataset_release_digest.empty() || run.code_digest.empty() || run.output_bundle_digest.empty() || !run.completed || !approve_retraining_input(run.dataset_release_digest)) return false;
        if (training_runs_.find(run.training_run_id) != training_runs_.end()) return false;
        training_runs_.emplace(run.training_run_id, run);
        return true;
    }

    bool register_offline_evaluation(const Stage14OfflineEvaluation& evaluation) {
        if (evaluation.evaluation_digest.empty() || evaluation.release_id.empty() || !evaluation.quality_pass || !evaluation.safety_pass || !evaluation.privacy_pass || !evaluation.security_pass) return false;
        if (releases_.find(evaluation.release_id) == releases_.end()) return false;
        evaluations_.emplace(evaluation.evaluation_digest, evaluation);
        return true;
    }

    bool authorize_lifecycle(const std::string& training_run_id, const std::string& evaluation_digest, const std::string& rollback_release_id) {
        const auto run = training_runs_.find(training_run_id);
        const auto evaluation = evaluations_.find(evaluation_digest);
        if (run == training_runs_.end() || evaluation == evaluations_.end() || releases_.find(rollback_release_id) == releases_.end() || evaluation->second.evaluation_digest.empty()) return false;
        lifecycle_ready_ = true;
        lifecycle_training_run_id_ = training_run_id;
        lifecycle_evaluation_digest_ = evaluation_digest;
        lifecycle_rollback_release_id_ = rollback_release_id;
        return true;
    }

    bool register_claim(const Stage14ClaimRecord& claim) {
        if (claim.claim_id.empty() || claim.claim_class.empty() || claim.evidence_digest.empty() || claim.release_id.empty() || claim.region.empty()) return false;
        if (unsupported_claims().find(claim.claim_class) != unsupported_claims().end()) return false;
        if (releases_.find(claim.release_id) == releases_.end() || regions_.find(claim.region) == regions_.end()) return false;
        claims_[claim.claim_id] = claim;
        return true;
    }

    bool claim_available(const std::string& claim_id, uint64_t now) const {
        const auto found = claims_.find(claim_id);
        if (found == claims_.end()) return false;
        const auto& claim = found->second;
        return !claim.withdrawn && claim.independently_reviewed && !claim.evidence_digest.empty() && now < claim.expires_at && active_release_matches(claim.release_id, claim.region);
    }

    bool claim_scope_alignment(const std::string& claim_id, const std::string& release_id, const std::string& region, uint64_t now) const {
        const auto found = claims_.find(claim_id);
        return found != claims_.end() && found->second.release_id == release_id && found->second.region == region && claim_available(claim_id, now);
    }

    bool register_emergency_change(const Stage14EmergencyChange& change) {
        if (change.change_id.empty() || change.mitigation_digest.empty() || change.region.empty() || change.owner.empty() || change.expires_at <= change.started_at || regions_.find(change.region) == regions_.end()) return false;
        if (emergency_changes_.find(change.change_id) != emergency_changes_.end()) return false;
        emergency_changes_.emplace(change.change_id, change);
        return true;
    }

    bool normalize_emergency_change(const std::string& change_id, const std::string& release_id) {
        const auto found = emergency_changes_.find(change_id);
        if (found == emergency_changes_.end() || releases_.find(release_id) == releases_.end()) return false;
        found->second.normalized = true;
        found->second.normalized_release_id = release_id;
        return true;
    }

    bool emergency_needs_normalization(const std::string& change_id, uint64_t now) const {
        const auto found = emergency_changes_.find(change_id);
        return found != emergency_changes_.end() && now >= found->second.expires_at && !found->second.normalized;
    }

    bool record_audit(const std::string& record_id, uint64_t timestamp, const std::string& tenant) {
        if (record_id.empty() || tenant.empty() || regions_for_tenant(tenant).empty()) return false;
        audit_records_.push_back({record_id, timestamp, tenant});
        return true;
    }

    bool audit_queryable(uint64_t retention_window, uint64_t now) const {
        if (audit_records_.empty()) return false;
        for (const auto& record : audit_records_) if (now < record.timestamp || now - record.timestamp > retention_window) return false;
        return true;
    }

    std::string create_backup() {
        last_backup_digest_ = "backup@" + std::to_string(stage10_hash_string(state_fingerprint()));
        return last_backup_digest_;
    }

    bool restore_backup(const std::string& backup_digest) const { return !last_backup_digest_.empty() && backup_digest == last_backup_digest_; }

    bool safe_route_after_failure(const std::string& region, bool fallback_available, bool controlled_denial_available) const {
        if (regions_.find(region) == regions_.end()) return false;
        return fallback_available || controlled_denial_available;
    }

    bool deny_tool_proposal(const std::string& proposal) const {
        static const std::vector<std::string> denied{"delete_all", "external_payment", "credential_exfiltration", "unbounded_rpc"};
        return std::any_of(denied.begin(), denied.end(), [&](const std::string& token) { return proposal.find(token) != std::string::npos; });
    }

    bool request_privacy_deletion(const std::string& tenant) {
        if (regions_for_tenant(tenant).empty()) return false;
        deletion_requests_.insert(tenant);
        return true;
    }

    bool deletion_completed(const std::string& tenant) const { return deletion_requests_.find(tenant) != deletion_requests_.end(); }

    bool region_halted(const std::string& region) const {
        const auto found = halted_.find(region);
        return found != halted_.end() && found->second;
    }
    bool configured() const { return configured_; }
    bool lifecycle_ready() const { return lifecycle_ready_; }
    size_t release_count() const { return releases_.size(); }
    size_t drift_count() const { return drift_signals_.size(); }
    size_t incident_count() const { return incidents_.size(); }
    size_t audit_count() const { return audit_records_.size(); }
    size_t promotion_count() const { return promotion_count_; }
    size_t rollback_count() const { return rollback_count_; }
    std::string active_release(const std::string& region) const { const auto found = active_release_.find(region); return found == active_release_.end() ? std::string() : found->second; }
    const std::vector<Stage14DriftSignal>& drift_signals() const { return drift_signals_; }

private:
    struct AuditRecord { std::string record_id; uint64_t timestamp; std::string tenant; };

    static bool complete_release(const Stage14ProductionRelease& release) {
        return !release.release_id.empty() && !release.bundle_digest.empty() && !release.region.empty() && !release.cohort_policy_digest.empty() && !release.evaluation_digest.empty() && !release.approval_digest.empty();
    }
    static bool same_release(const Stage14ProductionRelease& left, const Stage14ProductionRelease& right) {
        return left.release_id == right.release_id && left.bundle_digest == right.bundle_digest && left.region == right.region && left.cohort_policy_digest == right.cohort_policy_digest && left.evaluation_digest == right.evaluation_digest && left.approval_digest == right.approval_digest && left.deployed_at == right.deployed_at;
    }
    static double signed_threshold(const std::string& metric) {
        if (metric == "quality" || metric == "safety" || metric == "retrieval" || metric == "language") return 0.75;
        if (metric == "latency") return 100.0;
        if (metric == "cost") return 0.05;
        if (metric == "privacy" || metric == "security") return 0.0;
        return -1.0;
    }
    static bool breached(const std::string& metric, double observed, double threshold) {
        if (metric == "quality" || metric == "safety" || metric == "retrieval" || metric == "language") return observed < threshold;
        return observed > threshold;
    }
    static std::string severity_for(const std::string& metric) { return (metric == "privacy" || metric == "security") ? "critical" : ((metric == "quality" || metric == "safety" || metric == "retrieval" || metric == "language") ? "high" : "medium"); }
    static const std::set<std::string>& unsupported_claims() { static const std::set<std::string> claims{"general_intelligence", "human_level", "universal_safety", "unrestricted_autonomy", "consciousness"}; return claims; }
    bool closed_incident(const std::string& incident_id) const { const auto found = incidents_.find(incident_id); return found != incidents_.end() && found->second.disclosure_status == "decided" && found->second.root_cause_status != "unknown"; }
    bool active_release_matches(const std::string& release_id, const std::string& region) const { const auto found = active_release_.find(region); const auto halted = halted_.find(region); return found != active_release_.end() && found->second == release_id && (halted == halted_.end() || !halted->second); }
    std::string regions_for_tenant(const std::string& tenant) const { for (const auto& entry : regions_) if (std::find(entry.second.allowed_tenants.begin(), entry.second.allowed_tenants.end(), tenant) != entry.second.allowed_tenants.end()) return entry.first; return {}; }
    std::string state_fingerprint() const { return std::to_string(releases_.size()) + ":" + std::to_string(incidents_.size()) + ":" + std::to_string(audit_records_.size()) + ":" + std::to_string(promotion_count_); }

    bool configured_ = false;
    std::map<std::string, Stage14RegionalPolicy> regions_;
    std::map<std::string, Stage14ProductionRelease> releases_;
    std::map<std::string, std::string> active_release_;
    std::map<std::string, bool> halted_;
    std::map<std::string, std::string> halt_reasons_;
    std::vector<Stage14DriftSignal> drift_signals_;
    std::map<std::string, Stage14IncidentRecord> incidents_;
    std::map<std::string, Stage14DatasetRelease> datasets_;
    std::map<std::string, Stage14TrainingRun> training_runs_;
    std::map<std::string, Stage14OfflineEvaluation> evaluations_;
    std::map<std::string, Stage14ClaimRecord> claims_;
    std::map<std::string, Stage14EmergencyChange> emergency_changes_;
    std::vector<AuditRecord> audit_records_;
    std::set<std::string> deletion_requests_;
    std::string last_backup_digest_;
    std::string lifecycle_training_run_id_;
    std::string lifecycle_evaluation_digest_;
    std::string lifecycle_rollback_release_id_;
    bool lifecycle_ready_ = false;
    size_t promotion_count_ = 0U;
    size_t rollback_count_ = 0U;
};

} // namespace genesis
