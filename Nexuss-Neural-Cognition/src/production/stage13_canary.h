#pragma once

#include "production/stage12_serving.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace genesis {

struct Stage13CanaryCohort {
    std::string cohort_id;
    uint32_t maximum_users = 0U;
    std::vector<std::string> permitted_tasks;
    std::vector<std::string> excluded_domains;
    std::string consent_version;
    double traffic_fraction = 0.0;
    bool consequential_actions_disabled = true;
};

struct Stage13ConsentReceipt {
    std::string receipt_id;
    std::string user_hash;
    std::string cohort_id;
    std::string consent_version;
    std::string privacy_notice_digest;
    std::string scope_text;
    std::string support_path;
    bool consented = false;
    bool withdrawn = false;
};

struct Stage13RequestRecord {
    std::string request_hash;
    std::string user_hash;
    std::string cohort_id;
    std::string task_family;
    std::string domain;
    bool risk_triggered = false;
    bool routed = false;
};

struct Stage13HumanReviewRecord {
    std::string review_id;
    std::string request_hash;
    std::string model_bundle_digest;
    std::string rubric_version;
    std::string reviewer_group;
    std::string redacted_input;
    std::string outcome;
    uint32_t severity = 0U;
    bool adjudicated = false;
    bool expert_review = false;
};

struct Stage13IncidentRecord {
    std::string incident_id;
    std::string request_hash;
    uint32_t severity = 0U;
    std::string owner;
    std::string timeline;
    std::string containment;
    std::string decision_status;
    std::string regression_candidate;
    bool disclosed = false;
};

struct Stage13RedTeamFinding {
    std::string finding_id;
    std::string category;
    uint32_t severity = 0U;
    std::string attack_digest;
    std::string mitigation;
    std::string retest_result;
    bool critical = false;
    bool closed = false;
};

struct Stage13IndependentReview {
    std::string review_id;
    std::string reviewer_group;
    std::string scope_digest;
    std::string reproducibility_digest;
    std::string claims_boundary;
    bool security_reviewed = false;
    bool model_reviewed = false;
    bool data_governance_reviewed = false;
    bool metrics_reproduced = false;
};

struct Stage13ReleaseDecision {
    std::string decision;
    std::string rationale;
    std::vector<std::string> evidence_digests;
    std::vector<std::string> approvers;
};

class Stage13CanaryController {
public:
    bool configure(const Stage13CanaryCohort& cohort) {
        if (cohort.cohort_id.empty() || cohort.maximum_users == 0U || cohort.maximum_users > 25U || cohort.consent_version.empty() || cohort.traffic_fraction <= 0.0 || cohort.traffic_fraction > 0.01 || !cohort.consequential_actions_disabled || cohort.permitted_tasks.empty() || cohort.excluded_domains.empty()) return false;
        cohort_ = cohort;
        configured_ = true;
        paused_ = true;
        return true;
    }

    bool enroll(const Stage13ConsentReceipt& receipt) {
        if (!configured_ || receipt.receipt_id.empty() || receipt.user_hash.empty() || receipt.cohort_id != cohort_.cohort_id || receipt.consent_version != cohort_.consent_version || receipt.privacy_notice_digest.empty() || receipt.scope_text.empty() || receipt.support_path.empty() || !receipt.consented || receipt.withdrawn || users_.size() >= cohort_.maximum_users) return false;
        users_[receipt.user_hash] = receipt;
        return true;
    }

    bool withdraw(const std::string& user_hash) {
        const auto found = users_.find(user_hash);
        if (found == users_.end()) return false;
        found->second.withdrawn = true;
        return true;
    }

    bool route(const Stage13RequestRecord& request) {
        if (!configured_ || paused_ || users_.find(request.user_hash) == users_.end() || users_.at(request.user_hash).withdrawn || request.cohort_id != cohort_.cohort_id || !allowed_task(request.task_family) || excluded_domain(request.domain) || request.request_hash.empty()) return false;
        const double bucket = static_cast<double>(stage10_hash_string(request.request_hash) % 10000U) / 10000.0;
        if (bucket >= cohort_.traffic_fraction) return false;
        routed_.push_back(request);
        return true;
    }

    bool pause(const std::string& rationale) { if (rationale.empty()) return false; paused_ = true; pause_reason_ = rationale; return true; }
    bool resume_for_simulation() { if (!configured_ || users_.empty() || !pause_reason_.empty()) return false; paused_ = false; return true; }
    bool rollback(const std::string& incident_id) { if (incident_id.empty()) return false; paused_ = true; rollback_incident_id_ = incident_id; return true; }
    bool is_paused() const { return paused_; }
    bool configured() const { return configured_; }
    size_t enrolled_users() const { return users_.size(); }
    size_t routed_requests() const { return routed_.size(); }
    const Stage13CanaryCohort& cohort() const { return cohort_; }

    void reset_pause_for_test() { pause_reason_.clear(); paused_ = true; }

private:
    bool allowed_task(const std::string& task) const { return std::find(cohort_.permitted_tasks.begin(), cohort_.permitted_tasks.end(), task) != cohort_.permitted_tasks.end(); }
    bool excluded_domain(const std::string& domain) const { return std::find(cohort_.excluded_domains.begin(), cohort_.excluded_domains.end(), domain) != cohort_.excluded_domains.end(); }
    Stage13CanaryCohort cohort_;
    bool configured_ = false;
    bool paused_ = true;
    std::string pause_reason_;
    std::string rollback_incident_id_;
    std::map<std::string, Stage13ConsentReceipt> users_;
    std::vector<Stage13RequestRecord> routed_;
};

class Stage13ReviewLedger {
public:
    bool add_review(const Stage13HumanReviewRecord& review) {
        static const std::set<std::string> outcomes{"acceptable", "incorrect", "unsafe", "privacy", "policy", "escalation"};
        if (review.review_id.empty() || review.request_hash.empty() || review.model_bundle_digest.empty() || review.rubric_version.empty() || review.reviewer_group.empty() || review.redacted_input.empty() || review.redacted_input.find("RAW_SECRET") != std::string::npos || review.redacted_input.find("raw_email@example.com") != std::string::npos || outcomes.find(review.outcome) == outcomes.end()) return false;
        if ((review.outcome == "unsafe" || review.outcome == "privacy" || review.severity >= 3U) && !review.expert_review) return false;
        reviews_.push_back(review);
        access_audit_.push_back("review_access@" + std::to_string(stage10_hash_string(review.review_id + review.reviewer_group)));
        return true;
    }

    bool add_incident(const Stage13IncidentRecord& incident) {
        if (incident.incident_id.empty() || incident.request_hash.empty() || incident.severity == 0U || incident.owner.empty() || incident.timeline.empty() || incident.containment.empty() || incident.decision_status.empty()) return false;
        incidents_.push_back(incident);
        return true;
    }

    bool add_red_team(const Stage13RedTeamFinding& finding) {
        if (finding.finding_id.empty() || finding.category.empty() || finding.attack_digest.empty() || finding.mitigation.empty() || finding.retest_result.empty() || (finding.critical && !finding.closed)) return false;
        red_team_.push_back(finding);
        return true;
    }

    bool add_independent_review(const Stage13IndependentReview& review) {
        if (review.review_id.empty() || review.reviewer_group.empty() || review.scope_digest.empty() || review.reproducibility_digest.empty() || review.claims_boundary.empty()) return false;
        independent_reviews_.push_back(review);
        return true;
    }

    double quality_rate() const { return rate([](const Stage13HumanReviewRecord& review) { return review.outcome == "acceptable"; }); }
    double grounding_rate() const { return rate([](const Stage13HumanReviewRecord& review) { return review.outcome == "acceptable" || review.outcome == "incorrect"; }); }
    double severe_rate() const { return reviews_.empty() ? 0.0 : static_cast<double>(std::count_if(reviews_.begin(), reviews_.end(), [](const Stage13HumanReviewRecord& review) { return review.severity >= 3U; })) / static_cast<double>(reviews_.size()); }
    uint32_t unresolved_severity_one() const { return static_cast<uint32_t>(std::count_if(incidents_.begin(), incidents_.end(), [](const Stage13IncidentRecord& incident) { return incident.severity == 1U && incident.decision_status != "closed"; })); }
    uint32_t unresolved_critical_red_team() const { return static_cast<uint32_t>(std::count_if(red_team_.begin(), red_team_.end(), [](const Stage13RedTeamFinding& finding) { return finding.critical && !finding.closed; })); }
    bool all_reviews_redacted() const { return !reviews_.empty() && std::all_of(reviews_.begin(), reviews_.end(), [](const Stage13HumanReviewRecord& review) { return review.redacted_input.find("RAW_") == std::string::npos && review.redacted_input.find("@") == std::string::npos; }); }
    bool reviewer_access_audited() const { return access_audit_.size() == reviews_.size() && !access_audit_.empty(); }
    bool independent_reproducible() const { return !independent_reviews_.empty() && std::all_of(independent_reviews_.begin(), independent_reviews_.end(), [](const Stage13IndependentReview& review) { return review.security_reviewed && review.model_reviewed && review.data_governance_reviewed && review.metrics_reproduced; }); }
    size_t review_count() const { return reviews_.size(); }
    size_t incident_count() const { return incidents_.size(); }
    size_t red_team_count() const { return red_team_.size(); }
    const std::vector<Stage13HumanReviewRecord>& reviews() const { return reviews_; }
    const std::vector<Stage13IncidentRecord>& incidents() const { return incidents_; }
    const std::vector<Stage13RedTeamFinding>& red_team() const { return red_team_; }

private:
    template <typename F> double rate(F&& predicate) const { return reviews_.empty() ? 0.0 : static_cast<double>(std::count_if(reviews_.begin(), reviews_.end(), predicate)) / static_cast<double>(reviews_.size()); }
    std::vector<Stage13HumanReviewRecord> reviews_;
    std::vector<Stage13IncidentRecord> incidents_;
    std::vector<Stage13RedTeamFinding> red_team_;
    std::vector<Stage13IndependentReview> independent_reviews_;
    std::vector<std::string> access_audit_;
};

} // namespace genesis
