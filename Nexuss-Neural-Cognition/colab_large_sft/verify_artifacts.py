#!/usr/bin/env python3
"""Verify governed data and completed Colab SFT artifacts."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_rows(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open(encoding="utf-8") as handle:
        for line in handle:
            if line.strip():
                rows.append(json.loads(line))
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=Path("config.json"))
    parser.add_argument("--data-dir", type=Path, default=Path("artifacts/colab-large-sft/data"))
    parser.add_argument("--run-dir", type=Path, default=Path("artifacts/colab-large-sft/run"))
    parser.add_argument("--require-run", action="store_true")
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding="utf-8"))
    manifest = json.loads((args.data_dir / "release_manifest.json").read_text(encoding="utf-8"))
    train = read_rows(args.data_dir / "train.jsonl")
    validation = read_rows(args.data_dir / "validation.jsonl")
    test = read_rows(args.data_dir / "test.jsonl")
    ids = {name: {row["prompt_id"] for row in rows} for name, rows in [("train", train), ("validation", validation), ("test", test)]}
    checks: dict[str, dict[str, Any]] = {}

    def check(name: str, passed: bool, detail: str, value: Any = None) -> None:
        checks[name] = {"passed": bool(passed), "detail": detail, "value": value}

    check("dataset_identity", manifest["dataset_id"] == config["dataset_id"] and manifest["dataset_license"] == config["dataset_license"], "dataset and license match configuration", manifest["dataset_id"])
    check("split_counts", manifest["derived_splits"] == {"train": len(train), "validation": len(validation), "test": len(test)}, "manifest counts match JSONL rows", manifest["derived_splits"])
    check("split_disjointness", not (ids["train"] & ids["validation"] or ids["train"] & ids["test"] or ids["validation"] & ids["test"]), "train, validation, and test prompt IDs are disjoint")
    check("test_custody", manifest["test_method"] == "published_test_sft_untouched_except_governance_filter" and manifest["heldout_used_for_training"] is False, "published test split remains evaluation-only")
    check("production_boundary", manifest["production_allowed"] is False and config["production_allowed"] is False, "production release remains disabled")
    quarantine = read_rows(args.data_dir / "quarantine.jsonl")
    check("quarantine_recorded", manifest["quarantined_rows"] == len(quarantine), "quarantine count matches quarantine JSONL", len(quarantine))

    run_summary_path = args.run_dir / "run_summary.json"
    if args.require_run:
        check("run_summary_present", run_summary_path.exists(), "completed run summary exists")
        if run_summary_path.exists():
            summary = json.loads(run_summary_path.read_text(encoding="utf-8"))
            validation_metrics = summary.get("validation_metrics", {})
            test_metrics = summary.get("test_metrics", {})
            validation_loss = validation_metrics.get("validation_loss")
            test_loss = test_metrics.get("test_loss")
            check("evaluation_metrics", all(isinstance(value, (float, int)) and math.isfinite(float(value)) for value in [validation_loss, test_loss]), "finite validation and test losses recorded", {"validation_loss": validation_loss, "test_loss": test_loss})
            check("run_heldout_custody", summary.get("heldout_used_for_training") is False, "run summary records held-out exclusion")
            check("adapter_checkpoint", (args.run_dir / "adapter").exists(), "LoRA adapter checkpoint directory exists")
            probe_path = args.run_dir / "generation_probes.jsonl"
            if probe_path.exists():
                probes = read_rows(probe_path)
                secret_count = sum(1 for row in probes if row.get("secret_pattern"))
                action_count = sum(1 for row in probes if row.get("direct_tool_action_pattern"))
                check("probe_safety", secret_count == 0 and action_count == 0, "fixed probes contain no secret disclosure or direct tool-action pattern", {"secret_pattern_count": secret_count, "direct_tool_action_pattern_count": action_count})
            else:
                check("probe_safety", False, "generation probe file is missing")
            artifact_manifest_path = args.run_dir / "artifact_manifest.json"
            if artifact_manifest_path.exists():
                artifact_manifest = json.loads(artifact_manifest_path.read_text(encoding="utf-8"))
                hash_results = {}
                for relative, expected in artifact_manifest.get("artifact_sha256", {}).items():
                    file_path = args.run_dir / relative
                    hash_results[relative] = file_path.exists() and sha256_file(file_path) == expected
                check("artifact_hashes", all(hash_results.values()), "all recorded run artifact hashes verify", {"checked": len(hash_results), "failed": [key for key, value in hash_results.items() if not value]})
            else:
                check("artifact_hashes", False, "artifact manifest is missing")

    result = {"passed": all(item["passed"] for item in checks.values()), "checks": checks, "config": config, "manifest": manifest}
    args.run_dir.mkdir(parents=True, exist_ok=True)
    (args.run_dir / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
