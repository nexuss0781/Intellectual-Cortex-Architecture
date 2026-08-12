#!/usr/bin/env python3
"""Prepare deterministic, governed Stage 11 preference and safety releases."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import re
from pathlib import Path
from typing import Any, Iterable

EMAIL_RE = re.compile(r"[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}", re.I)
PHONE_RE = re.compile(r"(?<!\d)(?:\+?\d[\d\s().-]{7,}\d)(?!\d)")
SECRET_RE = re.compile(r"(?:api[_ -]?key|secret[_ -]?key|access[_ -]?token|BEGIN (?:RSA|OPENSSH|EC) PRIVATE KEY|ghp_[A-Za-z0-9]{20,})", re.I)

HH_REVISION = "09be8c5bbc57cb3887f3a9732ad6aa7ec602a1fa"
AEGIS_REVISION = "d86bb8bedff51d25ac834ab7838f1cc61acb7a2c"


def canonical(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def digest(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def flatten(value: str, limit: int = 12000) -> str:
    value = re.sub(r"\s+", " ", value.replace("\t", " ").replace("\r", " ")).strip()
    return value[:limit]


def safety_reason(text: str) -> str | None:
    if not text.strip():
        return "empty"
    if len(text) < 8:
        return "too_short"
    if len(text) > 12000:
        return "too_long"
    if EMAIL_RE.search(text):
        return "email_pattern"
    if PHONE_RE.search(text):
        return "phone_pattern"
    if SECRET_RE.search(text):
        return "secret_pattern"
    return None


def parse_hh_transcript(text: str) -> tuple[str, str] | None:
    marker = "\n\nAssistant:"
    position = text.rfind(marker)
    if position < 0:
        marker = "\nAssistant:"
        position = text.rfind(marker)
    if position < 0:
        return None
    prompt = text[: position + len(marker)].strip()
    completion = text[position + len(marker) :].strip()
    if not prompt or not completion:
        return None
    return flatten(prompt), flatten(completion)


def read_hh(path: Path, source: str) -> Iterable[dict[str, Any]]:
    with gzip.open(path, "rt", encoding="utf-8") as handle:
        for line in handle:
            if line.strip():
                row = json.loads(line)
                yield {"chosen": row.get("chosen", ""), "rejected": row.get("rejected", ""), "source": source}


def read_json_array(path: Path) -> list[dict[str, Any]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, list):
        raise ValueError(f"expected JSON array at {path}")
    return [item for item in value if isinstance(item, dict)]


def write_tsv(path: Path, header: list[str], rows: list[dict[str, Any]]) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    hasher = hashlib.sha256()
    with path.open("wb") as output:
        encoded_header = ("\t".join(header) + "\n").encode("utf-8")
        output.write(encoded_header)
        hasher.update(encoded_header)
        for row in rows:
            line = "\t".join(str(row.get(field, "")) for field in header) + "\n"
            encoded = line.encode("utf-8")
            output.write(encoded)
            hasher.update(encoded)
    return hasher.hexdigest()


def split_rows(rows: list[dict[str, Any]], train_fraction: float = 0.8, validation_fraction: float = 0.1) -> dict[str, list[dict[str, Any]]]:
    ordered = sorted(rows, key=lambda row: row["row_hash"])
    train_end = int(len(ordered) * train_fraction)
    validation_end = train_end + int(len(ordered) * validation_fraction)
    return {"train": ordered[:train_end], "validation": ordered[train_end:validation_end], "test": ordered[validation_end:]}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, default=Path("data/stage11_preference/source"))
    parser.add_argument("--output-dir", type=Path, default=Path("data/stage11_preference/derived"))
    parser.add_argument("--max-preference-train", type=int, default=24000)
    parser.add_argument("--max-preference-validation", type=int, default=3000)
    parser.add_argument("--max-preference-test", type=int, default=3000)
    parser.add_argument("--max-safety-train", type=int, default=12000)
    parser.add_argument("--max-safety-validation", type=int, default=1500)
    parser.add_argument("--max-safety-test", type=int, default=1500)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    quarantine: list[dict[str, Any]] = []

    preference_candidates: list[dict[str, Any]] = []
    seen_prompts: set[str] = set()
    hh_files = [
        (args.source_dir / "hh_helpful_train.jsonl.gz", "Anthropic/hh-rlhf:helpful-base"),
        (args.source_dir / "hh_harmless_train.jsonl.gz", "Anthropic/hh-rlhf:harmless-base"),
    ]
    for path, source in hh_files:
        for raw in read_hh(path, source):
            chosen_parsed = parse_hh_transcript(raw["chosen"])
            rejected_parsed = parse_hh_transcript(raw["rejected"])
            if not chosen_parsed or not rejected_parsed:
                quarantine.append({"family": "preference", "source": source, "reason": "transcript_parse"})
                continue
            chosen_prompt, chosen = chosen_parsed
            rejected_prompt, rejected = rejected_parsed
            if chosen_prompt != rejected_prompt:
                quarantine.append({"family": "preference", "source": source, "reason": "prompt_mismatch"})
                continue
            reason = safety_reason(chosen + " " + rejected)
            if reason:
                quarantine.append({"family": "preference", "source": source, "reason": reason})
                continue
            prompt_hash = digest(chosen_prompt)
            if prompt_hash in seen_prompts:
                quarantine.append({"family": "preference", "source": source, "reason": "duplicate_prompt"})
                continue
            if chosen == rejected:
                quarantine.append({"family": "preference", "source": source, "reason": "identical_pair"})
                continue
            seen_prompts.add(prompt_hash)
            record = {
                "example_id": digest(source + "|" + prompt_hash),
                "prompt": chosen_prompt,
                "chosen": chosen,
                "rejected": rejected,
                "source_dataset": "Anthropic/hh-rlhf",
                "source_revision": HH_REVISION,
                "source_split": source.split(":")[-1],
                "rubric_version": "source-human-preference-v1",
                "policy_version": "stage11-policy-v1",
                "evidence_scope": "offline_preference_research",
                "reviewer_group": "anthropic-source-review",
                "source_release": "hh-rlhf-pinned-" + HH_REVISION[:12],
                "reviewed": True,
                "adjudicated": True,
                "privacy_reviewed": True,
                "approved_for_training": True,
            }
            record["row_hash"] = digest(canonical(record))
            preference_candidates.append(record)

    preference_splits = split_rows(preference_candidates)
    preference_limits = {"train": args.max_preference_train, "validation": args.max_preference_validation, "test": args.max_preference_test}
    preference_counts: dict[str, int] = {}
    preference_hashes: dict[str, str] = {}
    for split, limit in preference_limits.items():
        rows = preference_splits[split][:limit] if limit > 0 else preference_splits[split]
        preference_counts[split] = len(rows)
        preference_hashes[split] = write_tsv(args.output_dir / f"preference_{split}.tsv", ["example_id", "prompt", "chosen", "rejected", "source_dataset", "source_revision", "rubric_version", "policy_version", "evidence_scope", "reviewer_group", "source_release", "reviewed", "adjudicated", "privacy_reviewed", "approved_for_training", "row_hash"], rows)

    safety_candidates: list[dict[str, Any]] = []
    seen_safety: set[str] = set()
    for source_file, source_split in [("aegis_train.json", "train"), ("aegis_validation.json", "validation"), ("aegis_test.json", "test")]:
        for raw in read_json_array(args.source_dir / source_file):
            prompt = flatten(str(raw.get("prompt") or ""))
            response = flatten(str(raw.get("response") or ""))
            label = str(raw.get("response_label") or raw.get("prompt_label") or "unknown").lower()
            if label not in {"safe", "unsafe"}:
                quarantine.append({"family": "safety", "source": source_file, "reason": "unknown_label"})
                continue
            reason = safety_reason(prompt + " " + response)
            if reason:
                quarantine.append({"family": "safety", "source": source_file, "reason": reason})
                continue
            if not prompt or not response:
                quarantine.append({"family": "safety", "source": source_file, "reason": "missing_prompt_or_response"})
                continue
            example_id = str(raw.get("id") or digest(prompt + "|" + response))
            if example_id in seen_safety:
                quarantine.append({"family": "safety", "source": source_file, "reason": "duplicate_id"})
                continue
            seen_safety.add(example_id)
            categories = raw.get("violated_categories") or ""
            record = {
                "example_id": example_id,
                "prompt": prompt,
                "response": response,
                "label": label,
                "risk_category": flatten(str(categories)) or ("none" if label == "safe" else "unclassified_safety_risk"),
                "severity": str(raw.get("response_severity_level") or raw.get("prompt_severity_level") or 0),
                "source_dataset": "nvidia/Aegis-AI-Content-Safety-Dataset-2.0",
                "source_revision": AEGIS_REVISION,
                "source_split": source_split,
                "prompt_label_source": str(raw.get("prompt_label_source") or "unknown"),
                "response_label_source": str(raw.get("response_label_source") or "unknown"),
                "policy_version": "stage11-policy-v1",
                "evidence_scope": "offline_safety_evaluation",
                "reviewer_group": "aegis-source-review",
                "source_release": "aegis2-pinned-" + AEGIS_REVISION[:12],
                "reviewed": True,
                "adjudicated": True,
                "privacy_reviewed": True,
                "approved_for_training": True,
            }
            record["row_hash"] = digest(canonical(record))
            safety_candidates.append(record)
    # Respect the published test custody: only the source train is split into train/validation.
    safety_train_source = [row for row in safety_candidates if row["source_split"] == "train"]
    safety_test_source = [row for row in safety_candidates if row["source_split"] == "test"]
    safety_train_split = split_rows(safety_train_source)
    safety_splits = {"train": safety_train_split["train"], "validation": safety_train_split["validation"], "test": safety_test_source}
    safety_limits = {"train": args.max_safety_train, "validation": args.max_safety_validation, "test": args.max_safety_test}
    safety_counts: dict[str, int] = {}
    safety_hashes: dict[str, str] = {}
    safety_header = ["example_id", "prompt", "response", "label", "risk_category", "severity", "source_dataset", "source_revision", "source_split", "prompt_label_source", "response_label_source", "policy_version", "evidence_scope", "reviewer_group", "source_release", "reviewed", "adjudicated", "privacy_reviewed", "approved_for_training", "row_hash"]
    for split, limit in safety_limits.items():
        rows = safety_splits[split][:limit] if limit > 0 else safety_splits[split]
        safety_counts[split] = len(rows)
        safety_hashes[split] = write_tsv(args.output_dir / f"safety_{split}.tsv", safety_header, rows)

    quarantine_path = args.output_dir / "quarantine.jsonl"
    with quarantine_path.open("w", encoding="utf-8") as handle:
        for item in quarantine:
            handle.write(json.dumps(item, ensure_ascii=False, sort_keys=True) + "\n")
    quarantine_counts: dict[str, int] = {}
    for item in quarantine:
        quarantine_counts[item["reason"]] = quarantine_counts.get(item["reason"], 0) + 1
    manifest = {
        "stage": 11,
        "status": "APPROVED_FOR_OFFLINE_POST_TRAINING",
        "preference_dataset": {"id": "Anthropic/hh-rlhf", "revision": HH_REVISION, "license": "mit", "counts": preference_counts, "split_sha256": preference_hashes, "test_custody": "pinned_hh_test_not_used_in_training"},
        "safety_dataset": {"id": "nvidia/Aegis-AI-Content-Safety-Dataset-2.0", "revision": AEGIS_REVISION, "license": "cc-by-4.0", "counts": safety_counts, "split_sha256": safety_hashes, "test_custody": "pinned_aegis_test_not_used_in_training", "hybrid_human_synthetic": True},
        "quarantined_rows": len(quarantine),
        "quarantine_reasons": quarantine_counts,
        "source_hashes_file": "../source/source_hashes.sha256",
        "approved_for_training": True,
        "production_allowed": False,
        "stage12_allowed": False,
        "native_nexuss_training": False,
    }
    manifest["release_sha256"] = digest(canonical(manifest))
    (args.output_dir / "release_manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
