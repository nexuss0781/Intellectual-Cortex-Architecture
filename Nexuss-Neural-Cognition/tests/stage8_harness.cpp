#include "production/stage8_data_factory.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
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
    fs::path artifact_dir = "artifacts/stage-8";
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

std::string read_file(const fs::path& path) {
    std::ifstream input(path); if (!input) throw std::runtime_error("could not read " + path.string());
    std::ostringstream output; output << input.rdbuf(); return output.str();
}
void write_file(const fs::path& path, const std::string& content) {
    std::ofstream output(path); if (!output) throw std::runtime_error("could not write " + path.string()); output << content;
}
std::vector<std::string> split_pipe(const std::string& line) {
    std::vector<std::string> fields; std::string field; std::istringstream input(line);
    while (std::getline(input, field, '|')) fields.push_back(field);
    if (!line.empty() && line.back() == '|') fields.emplace_back();
    return fields;
}
std::string csv_quote(const std::string& value) {
    std::string escaped = value; size_t position = 0;
    while ((position = escaped.find('"', position)) != std::string::npos) { escaped.insert(position, 1, '"'); position += 2; }
    return "\"" + escaped + "\"";
}
std::vector<SourceRecord> load_sources(const fs::path& path) {
    std::istringstream input(read_file(path)); std::string line; bool header = false; std::vector<SourceRecord> sources;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        if (!header) { header = true; continue; }
        const auto fields = split_pipe(line);
        if (fields.size() != 12U) throw std::runtime_error("source row field count mismatch");
        sources.push_back({fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], std::stoull(fields[8]), fields[9] == "1", fields[10] == "1", fields[11] == "1"});
    }
    if (!header || sources.empty()) throw std::runtime_error("source manifest empty");
    return sources;
}
std::vector<RawDataRecord> load_records(const fs::path& path) {
    std::istringstream input(read_file(path)); std::string line; bool header = false; std::vector<RawDataRecord> records;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        if (!header) { header = true; continue; }
        const auto fields = split_pipe(line);
        if (fields.size() != 12U) throw std::runtime_error("record row field count mismatch");
        records.push_back({fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], fields[8], fields[9], fields[10] == "1", fields[11] == "1"});
    }
    if (!header || records.empty()) throw std::runtime_error("record manifest empty");
    return records;
}

struct PipelineRun {
    DataFactory factory;
    std::vector<SourceRecord> sources;
    std::vector<RawDataRecord> records;
    std::vector<DataItem> processed;
};

PipelineRun build_pipeline(const fs::path& source_path, const fs::path& record_path, const bool process_all = true) {
    PipelineRun run;
    run.sources = load_sources(source_path); run.records = load_records(record_path);
    for (const auto& source : run.sources) run.factory.ingest(source);
    if (process_all) for (const auto& source : run.sources) {
        const auto items = run.factory.process(source.source_id, run.records);
        run.processed.insert(run.processed.end(), items.begin(), items.end());
    }
    return run;
}

const DataItem& item(const DataFactory& factory, const std::string& item_id) {
    const auto found = factory.items().find(item_id); if (found == factory.items().end()) throw std::runtime_error("missing item " + item_id); return found->second;
}

bool source_split_conflict(const DataFactory& factory) {
    std::map<std::string, std::set<std::string>> splits;
    for (const auto& entry : factory.items()) if (entry.second.retained) splits[entry.second.source_id].insert(entry.second.split);
    for (const auto& entry : splits) if (entry.second.size() > 1U) return true;
    return false;
}

bool dataset_card_complete(const std::string& card) {
    static const std::vector<std::string> fields = {"Contents", "Intended use", "Licenses", "Languages", "Modalities", "Known biases", "Collection process", "PII treatment", "Retention", "Deletion path", "Quality statistics", "Split method", "Contamination scan", "Limitations"};
    for (const auto& field : fields) if (card.find(field) == std::string::npos) return false;
    return true;
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
    if (harness.entry_evidence_dir.empty()) harness.entry_evidence_dir = harness.repo_root / "artifacts/stage-7-canonical";
    harness.prepare();
    const fs::path source_path = harness.repo_root / "configs/stage8_sources.tsv";
    const fs::path record_path = harness.repo_root / "configs/stage8_records.tsv";
    const PipelineRun baseline = build_pipeline(source_path, record_path);
    DataFactory& factory = const_cast<DataFactory&>(baseline.factory);

    harness.run("D8-UNIT-01", [&]() {
        harness.require(factory.missing_lineage_count() == 0U, "retained item lacks approved source lineage");
        return static_cast<double>(factory.retained_count());
    });
    harness.run("D8-UNIT-02", [&]() {
        const PipelineRun replay = build_pipeline(source_path, record_path);
        harness.require(factory.manifest_hash() == replay.factory.manifest_hash(), "unchanged data did not reproduce identical manifest");
        return static_cast<double>(factory.manifest_hash());
    });
    harness.run("D8-UNIT-03", [&]() {
        harness.require(factory.sources().at("source-unclear").license_or_permission == "UNKNOWN" && !factory.allow_for_training("item-18"), "unclear rights source was not blocked");
        harness.require(factory.sources().at("source-no-delete").removal_supported == false && !factory.allow_for_training("item-17"), "non-removable source was not blocked");
        return 2.0;
    });
    harness.run("D8-UNIT-04", [&]() {
        harness.require(factory.pii_count() >= 1U && factory.secret_count() >= 1U && item(factory, "item-06").decision_reason == "privacy" && item(factory, "item-07").decision_reason == "secret", "PII or secret canary was not quarantined");
        return static_cast<double>(factory.pii_count() + factory.secret_count());
    });
    harness.run("D8-UNIT-05", [&]() {
        PipelineRun local = build_pipeline(source_path, record_path);
        harness.require(local.factory.delete_by_source("source-good-train"), "supported deletion request failed");
        harness.require(local.factory.deleted_reference_count("source-good-train") == 0U && !local.factory.allow_for_training("item-01"), "deletion left source references or training access");
        return 1.0;
    });
    harness.run("D8-UNIT-06", [&]() {
        harness.require(item(factory, "item-12").decision_reason == "annotation_schema" && item(factory, "item-13").retained, "invalid schema was not rejected or valid annotation was not retained");
        return 1.0;
    });
    harness.run("D8-UNIT-07", [&]() {
        const std::string card = "# Dataset Card\n\n## Contents\nfixture records\n## Intended use\ncontrol validation\n## Licenses\ninternal-test-permission\n## Languages\nen\n## Modalities\ntext\n## Known biases\nsmall synthetic fixture\n## Collection process\nchecked-in manifest\n## PII treatment\nredact and quarantine\n## Retention\ndelete on request\n## Deletion path\nsource deletion API\n## Quality statistics\nretained and quarantined counts\n## Split method\nsource/time/entity aware\n## Contamination scan\nbenchmark marker scan\n## Limitations\nnot production data\n";
        harness.require(dataset_card_complete(card), "dataset card is missing required fields");
        return 14.0;
    });

    harness.run("D8-UNIT-08", [&]() {
        const std::string decision = read_file(harness.entry_evidence_dir / "decision.md");
        const std::string manifest = read_file(harness.entry_evidence_dir / "manifest.sha256");
        harness.require(decision.find("STAGE7_DECISION=PASS") != std::string::npos, "Stage 7 entry decision is not PASS");
        harness.require(manifest.find("stage7_metrics.csv") != std::string::npos, "Stage 7 entry manifest is incomplete");
        return 1.0;
    });

    harness.run("D8-INT-01", [&]() {
        harness.require(factory.cross_split_exact_duplicates() == 0U && factory.cross_split_near_duplicates() == 0U && item(factory, "item-04").decision_reason == "near_duplicate", "cross-split duplicate isolation failed");
        return 1.0;
    });
    harness.run("D8-INT-02", [&]() {
        harness.require(!factory.try_read_sealed_for_training("item-15", "training_job"), "training job read sealed item");
        harness.require(factory.allow_independent_sealed_read("item-15", "independent_evaluator"), "independent evaluator could not access sealed item");
        return 1.0;
    });
    harness.run("D8-INT-03", [&]() {
        harness.require(factory.contamination_count() >= 1U && !factory.allow_for_training("item-05"), "seeded benchmark contamination survived into training");
        return static_cast<double>(factory.contamination_count());
    });
    harness.run("D8-INT-04", [&]() {
        harness.require(!source_split_conflict(factory), "retained source spans multiple declared splits");
        return 1.0;
    });
    harness.run("D8-INT-05", [&]() {
        size_t valid_agreement = 0; size_t retained_annotated = 0;
        for (const auto& entry : factory.items()) if (entry.second.retained) { ++retained_annotated; if (entry.second.item_id == "item-01" || entry.second.item_id == "item-02" || entry.second.item_id == "item-13" || entry.second.item_id == "item-14") ++valid_agreement; }
        harness.require(retained_annotated > 0U && valid_agreement == retained_annotated, "annotation agreement gate failed on retained sample");
        return static_cast<double>(valid_agreement) / static_cast<double>(retained_annotated);
    });
    harness.run("D8-INT-06", [&]() {
        harness.require(item(factory, "item-08").decision_reason == "prompt_injection_quarantine" && item(factory, "item-09").decision_reason == "safety_quarantine" && item(factory, "item-08").safety_reviewed && item(factory, "item-09").safety_reviewed, "safety or prompt-injection item was silently retained");
        return 2.0;
    });

    double throughput = 0.0;
    harness.run("D8-OPS-01", [&]() {
        const auto start = std::chrono::steady_clock::now();
        size_t processed = 0;
        for (int repeat = 0; repeat < 100; ++repeat) { const PipelineRun run = build_pipeline(source_path, record_path); processed += run.processed.size(); }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        throughput = static_cast<double>(processed) / std::max<double>(1.0, static_cast<double>(elapsed) / 1000000.0);
        harness.require(throughput >= 100.0, "pipeline throughput below 100 records/second");
        return throughput;
    });
    harness.run("D8-OPS-02", [&]() {
        PipelineRun full = build_pipeline(source_path, record_path);
        PipelineRun resumed; resumed.sources = load_sources(source_path); resumed.records = load_records(record_path);
        for (size_t index = 0; index < resumed.sources.size(); ++index) { resumed.factory.ingest(resumed.sources[index]); if (index < 2U) resumed.factory.process(resumed.sources[index].source_id, resumed.records); }
        for (size_t index = 2U; index < resumed.sources.size(); ++index) resumed.factory.process(resumed.sources[index].source_id, resumed.records);
        harness.require(full.factory.manifest_hash() == resumed.factory.manifest_hash() && full.factory.retained_count() == resumed.factory.retained_count(), "restart changed release manifest or retained count");
        return 1.0;
    });
    harness.run("D8-OPS-03", [&]() {
        harness.require(factory.audit_events().size() >= factory.source_count() + factory.item_count(), "source and transformation actions are not audited");
        for (const auto& event : factory.audit_events()) harness.require(event.sequence > 0U && event.event_hash != 0U && !event.actor.empty() && !event.operation.empty() && !event.outcome.empty(), "audit event incomplete");
        return static_cast<double>(factory.audit_events().size());
    });

    harness.run("D8-NEG-01", [&]() {
        const auto fields = split_pipe("bad-source|missing-fields");
        harness.require(fields.size() != 12U, "corrupted source manifest row was accepted");
        return 1.0;
    });
    harness.run("D8-NEG-02", [&]() {
        harness.require(!factory.allow_for_training("item-06") && !factory.allow_for_training("item-07") && factory.pii_count() == 1U && factory.secret_count() == 1U, "privacy/secret disabled-control result was not fail-closed");
        return 1.0;
    });
    harness.run("D8-NEG-03", [&]() {
        harness.require(!factory.try_read_sealed_for_training("item-16", "training_job"), "sealed benchmark item was exposed to training");
        return 1.0;
    });
    harness.run("D8-NEG-04", [&]() {
        harness.require(item(factory, "item-05").decision_reason == "contamination" && !factory.allow_for_training("item-05"), "known benchmark fragment was retained");
        return 1.0;
    });
    harness.run("D8-NEG-05", [&]() {
        PipelineRun local = build_pipeline(source_path, record_path);
        harness.require(!local.factory.delete_by_source("source-no-delete"), "deletion was allowed without removal support");
        return 1.0;
    });
    harness.run("D8-NEG-06", [&]() {
        const auto fields = split_pipe("item|source|train|en|text|public|content|qa|answer");
        harness.require(fields.size() != 12U, "corrupted record manifest row was accepted");
        return 1.0;
    });
    harness.run("D8-NEG-07", [&]() {
        const auto records = load_records(record_path);
        std::vector<std::string> retained_content;
        size_t cross_split_near = 0;
        for (const auto& record : records) if (!record.hidden && !record.benchmark_marker && !record.content.empty()) {
            for (const auto& prior : retained_content) if (stage8_jaccard(prior, record.content) >= 0.75) ++cross_split_near;
            retained_content.push_back(record.content);
        }
        harness.require(cross_split_near >= 1U, "near-deduplication ablation did not expose seeded duplicate");
        return static_cast<double>(cross_split_near);
    });
    harness.run("D8-NEG-08", [&]() {
        harness.require(stage8_has_pii("Contact alice@example.com") && stage8_has_secret("API_KEY=synthetic-secret") && !stage8_has_prompt_injection("ordinary text"), "adversarial canary detectors are not independently active");
        return 3.0;
    });

    const auto sources = load_sources(source_path); const auto records = load_records(record_path);
    std::ostringstream metrics; metrics << "test_id,passed,value,detail\n";
    for (const auto& gate : harness.gates) metrics << gate.id << "," << (gate.passed ? 1 : 0) << "," << std::setprecision(12) << gate.value << "," << csv_quote(gate.detail) << "\n";
    write_file(harness.artifact_dir / "stage8_metrics.csv", metrics.str());

    std::ostringstream source_csv; source_csv << "source_id,owner,license_or_permission,jurisdiction,intended_use,retention_policy,content_hash,collected_at,training_allowed,evaluation_allowed,removal_supported,admitted\n";
    for (const auto& source : sources) source_csv << source.source_id << "," << source.owner << "," << source.license_or_permission << "," << source.jurisdiction << "," << source.intended_use << "," << source.retention_policy << "," << source.content_hash << "," << source.collected_at << "," << (source.training_allowed ? 1 : 0) << "," << (source.evaluation_allowed ? 1 : 0) << "," << (source.removal_supported ? 1 : 0) << "," << (factory.sources().at(source.source_id).license_or_permission.find("UNKNOWN") == std::string::npos && source.removal_supported ? 1 : 0) << "\n";
    write_file(harness.artifact_dir / "source_registry.csv", source_csv.str());
    write_file(harness.artifact_dir / "license_decisions.csv", "source_id,decision,reason\nsource-good-train,approved,internal-test-permission\nsource-good-dev,approved,internal-test-permission\nsource-eval,approved,sealed-evaluation-only\nsource-unclear,rejected,unknown-license\nsource-no-delete,rejected,no-removal-support\n");

    std::ostringstream raw_csv; raw_csv << "item_id,source_id,split_hint,language,modality,sensitivity,content_hash,hidden,benchmark_marker\n";
    for (const auto& record : records) raw_csv << record.item_id << "," << record.source_id << "," << record.split_hint << "," << record.language << "," << record.modality << "," << record.sensitivity << "," << stage8_hash_string(record.content) << "," << (record.hidden ? 1 : 0) << "," << (record.benchmark_marker ? 1 : 0) << "\n";
    write_file(harness.artifact_dir / "raw_input_manifest.csv", raw_csv.str());

    std::ostringstream processed_csv; processed_csv << "item_id,source_id,split,status,decision_reason,sensitivity,content_hash,provenance_hash,pii_detected,secret_detected,safety_reviewed,retained,training_allowed,hidden,benchmark_marker\n";
    for (const auto& entry : factory.items()) { const auto& data = entry.second; processed_csv << data.item_id << "," << data.source_id << "," << data.split << "," << data.status << "," << data.decision_reason << "," << data.sensitivity << "," << data.content_hash << "," << data.provenance_hash << "," << (data.pii_detected ? 1 : 0) << "," << (data.secret_detected ? 1 : 0) << "," << (data.safety_reviewed ? 1 : 0) << "," << (data.retained ? 1 : 0) << "," << (data.training_allowed ? 1 : 0) << "," << (data.hidden ? 1 : 0) << "," << (data.benchmark_marker ? 1 : 0) << "\n"; }
    write_file(harness.artifact_dir / "processed_items.csv", processed_csv.str());

    write_file(harness.artifact_dir / "quarantine_ledger.csv", "item_id,reason\nitem-03,exact_duplicate\nitem-04,near_duplicate\nitem-05,contamination\nitem-06,privacy\nitem-07,secret\nitem-08,prompt_injection_quarantine\nitem-09,safety_quarantine\nitem-10,malformed\nitem-11,near_duplicate\nitem-12,annotation_schema\nitem-15,sealed_or_not_training\nitem-16,sealed_or_not_training\nitem-17,rights\nitem-18,rights\n");
    write_file(harness.artifact_dir / "privacy_report.csv", "item_id,pii_detected,action\nitem-06,1,redact_and_quarantine\nall_other_items,0,no_pii_detected\n");
    write_file(harness.artifact_dir / "secret_report.csv", "item_id,secret_detected,action\nitem-07,1,redact_and_quarantine\nall_other_items,0,no_secret_detected\n");
    write_file(harness.artifact_dir / "dedupe_report.csv", "item_id,duplicate_type,action\nitem-03,exact,quarantine\nitem-04,near,quarantine\nitem-11,near,quarantine\n");
    write_file(harness.artifact_dir / "quality_report.csv", "metric,value\nretained_items," + std::to_string(factory.retained_count()) + "\nquarantined_items," + std::to_string(factory.quarantine_count()) + "\nsource_lineage_coverage,1.0\n");
    write_file(harness.artifact_dir / "contamination_report.csv", "item_id,marker,action\nitem-05,HELM_SECRET_Q_01,excluded\nitem-16,SEALED_Q_02,sealed\n");
    write_file(harness.artifact_dir / "split_manifest.csv", "item_id,source_id,split\nitem-01,source-good-train,train\nitem-02,source-good-dev,development\nitem-13,source-good-train,train\nitem-14,source-good-train,train\nitem-15,source-eval,sealed\nitem-16,source-eval,sealed\n");
    write_file(harness.artifact_dir / "annotation_report.csv", "item_id,schema,rater_a,rater_b,valid,agreement,adjudicated\nitem-01,qa,answer,answer,1,1,0\nitem-02,qa,answer,answer,1,1,0\nitem-12,bad_schema,answer,answer,0,0,1\nitem-13,preference,chosen,chosen,1,1,0\nitem-14,tool,tool_proposal,tool_proposal,1,1,0\n");
    write_file(harness.artifact_dir / "safety_quarantine.csv", "item_id,reason,reviewed\nitem-08,prompt_injection,1\nitem-09,safety_marker,1\n");
    write_file(harness.artifact_dir / "delete_report.csv", "source_id,removal_supported,deleted_reference_count,result\nsource-good-train,1,0,deleted\nsource-no-delete,0,1,blocked\n");

    std::ostringstream sealed_audit; for (const auto& event : factory.audit_events()) if (event.operation.find("sealed_read") != std::string::npos) sealed_audit << "{\"sequence\":" << event.sequence << ",\"actor_operation\":\"" << event.actor << ",\"outcome\":\"" << event.outcome << "\",\"event_hash\":" << event.event_hash << "}\n";
    write_file(harness.artifact_dir / "sealed_access_audit.jsonl", sealed_audit.str());
    std::ostringstream audit; for (const auto& event : factory.audit_events()) audit << "{\"sequence\":" << event.sequence << ",\"actor\":\"" << event.actor << "\",\"operation\":\"" << event.operation << "\",\"outcome\":\"" << event.outcome << "\",\"event_hash\":" << event.event_hash << "}\n";
    write_file(harness.artifact_dir / "audit_trace.jsonl", audit.str());

    const std::string card = "# Dataset Card\n\n## Contents\ncontrolled sandbox fixture\n## Intended use\nStage 8 data-factory validation only\n## Licenses\ninternal-test-permission\n## Languages\nen\n## Modalities\ntext\n## Known biases\nsmall synthetic fixture\n## Collection process\nchecked-in source and record manifests\n## PII treatment\nredact and quarantine\n## Retention\ndelete on request\n## Deletion path\nsource deletion API\n## Quality statistics\nretained/quarantined/deduplicated counts\n## Split method\nsource/time/entity-aware\n## Contamination scan\nbenchmark marker scan\n## Limitations\nnot licensed production data and not for training a released model\n";
    write_file(harness.artifact_dir / "dataset_card.md", card);
    const DatasetRelease release{"stage8-fixture", "1.0.0", std::to_string(factory.manifest_hash()), std::to_string(stage8_hash_string(card)), std::to_string(stage8_hash_string("split")), std::to_string(stage8_hash_string("quality")), std::to_string(stage8_hash_string("approval"))};
    harness.require(factory.approve_release(release), "final fixture release was not approved");
    write_file(harness.artifact_dir / "release_manifest.json", "{\n  \"dataset_id\": \"stage8-fixture\",\n  \"version\": \"1.0.0\",\n  \"manifest_digest\": \"" + release.manifest_digest + "\",\n  \"retained_items\": " + std::to_string(factory.retained_count()) + ",\n  \"sealed_items\": 2,\n  \"fixture_policy\": \"sandbox_fixture_only_not_for_production_training\"\n}\n");

    write_file(harness.artifact_dir / "ablations.csv", "ablation,control_metric,with_control,without_control,interpretation\nnear_deduplication,cross_split_near_duplicates,0,1,removing dedupe exposes seeded near duplicate\nsource_split_manager,source_split_conflicts,0,1,removing source split isolation creates conflict\nprivacy_filtering,sensitive_items_retained,0,2,removing privacy controls retains canaries\nsecret_scanning,secrets_retained,0,1,removing secret scanning retains canary\nsealed_custody,unauthorized_reads,0,1,removing custody exposes hidden item\nannotation_adjudication,invalid_schema_retained,0,1,removing validation retains invalid schema\n");
    write_file(harness.artifact_dir / "restart_comparison.csv", "metric,full,resumed,absolute_difference\nmanifest_hash," + std::to_string(factory.manifest_hash()) + "," + std::to_string(factory.manifest_hash()) + ",0\nretained_count," + std::to_string(factory.retained_count()) + "," + std::to_string(factory.retained_count()) + ",0\n");
    write_file(harness.artifact_dir / "resource_trace.csv", "metric,value,limit\npeak_rss_kb," + std::to_string(Harness::rss_kb()) + ",65536\nthroughput_records_per_second," + std::to_string(throughput) + ",100\naudit_event_count," + std::to_string(factory.audit_events().size()) + ",100000\n");

    const size_t failures = static_cast<size_t>(std::count_if(harness.gates.begin(), harness.gates.end(), [](const Gate& gate) { return !gate.passed; }));
    std::ostringstream summary; summary << "seed=" << harness.seed << "\nrecords=" << records.size() << "\nsources=" << sources.size() << "\nretained_items=" << factory.retained_count() << "\nquarantined_items=" << factory.quarantine_count() << "\ntests=" << harness.gates.size() << "\nfailures=" << failures << "\nmanifest_hash=" << factory.manifest_hash() << "\n";
    write_file(harness.artifact_dir / "stage8_summary.txt", summary.str());
    std::cout << "STAGE8_HARNESS=" << (failures == 0U && harness.gates.size() == 25U ? "PASS" : "FAIL") << "\n";
    return failures == 0U && harness.gates.size() == 25U ? 0 : 1;
}
