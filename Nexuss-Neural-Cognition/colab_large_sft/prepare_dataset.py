#!/usr/bin/env python3
"""Prepare a governed, deterministic UltraChat 200k release for Colab SFT.

The published UltraChat test_sft split is never used for training or validation.
Validation is a deterministic 5% tail of train_sft after canonical row hashing.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path
from typing import Any

from datasets import load_dataset

EMAIL_RE = re.compile(r"[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}", re.I)
PHONE_RE = re.compile(r"(?<!\d)(?:\+?\d[\d\s().-]{7,}\d)(?!\d)")
SECRET_RE = re.compile(r"(?:api[_ -]?key|secret[_ -]?key|access[_ -]?token|BEGIN (?:RSA|OPENSSH|EC) PRIVATE KEY|ghp_[A-Za-z0-9]{20,})", re.I)


def canonical_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def sha256_text(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def row_text(messages: list[dict[str, str]]) -> str:
    return "\n".join(f"{message['role']}: {message['content']}" for message in messages)


def normalize_messages(raw: Any) -> list[dict[str, str]]:
    if not isinstance(raw, list):
        return []
    normalized: list[dict[str, str]] = []
    for item in raw:
        if not isinstance(item, dict):
            continue
        role = str(item.get("role", "")).strip().lower()
        content = str(item.get("content", ""))
        if role not in {"system", "user", "assistant"} or not content.strip():
            continue
        normalized.append({"role": role, "content": content.strip()})
    return normalized


def validate_messages(messages: list[dict[str, str]]) -> str | None:
    if len(messages) < 2:
        return "too_few_messages"
    if messages[-1]["role"] != "assistant":
        return "last_message_not_assistant"
    if not any(message["role"] == "user" for message in messages):
        return "no_user_message"
    text = row_text(messages)
    if len(text) < 8:
        return "too_short"
    if len(text) > 100_000:
        return "too_long"
    if EMAIL_RE.search(text):
        return "email_pattern"
    if PHONE_RE.search(text):
        return "phone_pattern"
    if SECRET_RE.search(text):
        return "secret_pattern"
    return None


def make_record(row: dict[str, Any], split: str, license_name: str, dataset_id: str) -> tuple[dict[str, Any] | None, str | None]:
    messages = normalize_messages(row.get("messages"))
    reason = validate_messages(messages)
    prompt_id = str(row.get("prompt_id", "")).strip()
    if not prompt_id:
        prompt_id = sha256_text(canonical_json(messages))
    if reason:
        return None, reason
    record = {
        "prompt_id": prompt_id,
        "messages": messages,
        "source_dataset": dataset_id,
        "source_license": license_name,
        "split_source": split,
    }
    record["row_hash"] = sha256_text(canonical_json(record))
    return record, None


def write_jsonl(path: Path, rows: list[dict[str, Any]], split: str) -> str:
    digest = hashlib.sha256()
    with path.open("wb") as handle:
        for row in rows:
            output = json.dumps({**row, "split": split}, ensure_ascii=False, sort_keys=True) + "\n"
            encoded = output.encode("utf-8")
            handle.write(encoded)
            digest.update(encoded)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=Path("config.json"))
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/colab-large-sft/data"))
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding="utf-8"))
    output_dir = args.output_dir
    if output_dir.exists() and args.force:
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = output_dir / "release_manifest.json"
    if manifest_path.exists() and not args.force:
        print(f"Using existing governed release: {manifest_path}")
        return

    dataset_id = config["dataset_id"]
    revision = config["dataset_revision"]
    license_name = config["dataset_license"]
    train_source = load_dataset(dataset_id, split=config["train_split"], revision=revision, trust_remote_code=False)
    test_source = load_dataset(dataset_id, split=config["test_split"], revision=revision, trust_remote_code=False)

    quarantined: list[dict[str, Any]] = []
    seen: set[str] = set()
    train_rows: list[dict[str, Any]] = []
    for row in train_source:
        record, reason = make_record(row, config["train_split"], license_name, dataset_id)
        if record is None:
            quarantined.append({"split": config["train_split"], "reason": reason, "prompt_id": row.get("prompt_id")})
            continue
        if record["prompt_id"] in seen:
            quarantined.append({"split": config["train_split"], "reason": "duplicate_prompt_id", "prompt_id": record["prompt_id"]})
            continue
        seen.add(record["prompt_id"])
        train_rows.append(record)

    test_rows: list[dict[str, Any]] = []
    test_seen: set[str] = set()
    for row in test_source:
        record, reason = make_record(row, config["test_split"], license_name, dataset_id)
        if record is None:
            quarantined.append({"split": config["test_split"], "reason": reason, "prompt_id": row.get("prompt_id")})
            continue
        if record["prompt_id"] in test_seen or record["prompt_id"] in seen:
            quarantined.append({"split": config["test_split"], "reason": "duplicate_prompt_id_across_release", "prompt_id": record["prompt_id"]})
            continue
        test_seen.add(record["prompt_id"])
        test_rows.append(record)

    train_rows.sort(key=lambda row: row["row_hash"])
    test_rows.sort(key=lambda row: row["row_hash"])
    validation_fraction = float(config["validation_fraction_of_train_sft"])
    validation_count = max(1, int(len(train_rows) * validation_fraction))
    validation_rows = train_rows[-validation_count:]
    final_train_rows = train_rows[:-validation_count]

    if config.get("max_train_examples", 0):
        final_train_rows = final_train_rows[: int(config["max_train_examples"])]
    if config.get("max_validation_examples", 0):
        validation_rows = validation_rows[: int(config["max_validation_examples"])]
    if config.get("max_test_examples", 0):
        test_rows = test_rows[: int(config["max_test_examples"])]

    split_hashes = {
        "train": write_jsonl(output_dir / "train.jsonl", final_train_rows, "train"),
        "validation": write_jsonl(output_dir / "validation.jsonl", validation_rows, "validation"),
        "test": write_jsonl(output_dir / "test.jsonl", test_rows, "test"),
    }
    quarantine_path = output_dir / "quarantine.jsonl"
    with quarantine_path.open("w", encoding="utf-8") as handle:
        for row in quarantined:
            handle.write(json.dumps(row, ensure_ascii=False, sort_keys=True) + "\n")
    quarantine_counts: dict[str, int] = {}
    for row in quarantined:
        reason = str(row.get("reason"))
        quarantine_counts[reason] = quarantine_counts.get(reason, 0) + 1
    manifest = {
        "dataset_id": dataset_id,
        "dataset_url": config["dataset_url"],
        "dataset_revision": revision,
        "dataset_license": license_name,
        "base_model_id": config["base_model_id"],
        "base_model_license": config["base_model_license"],
        "source_splits": {"train_sft": len(train_source), "test_sft": len(test_source)},
        "derived_splits": {"train": len(final_train_rows), "validation": len(validation_rows), "test": len(test_rows)},
        "validation_method": "deterministic_hash_sorted_tail_of_train_sft",
        "test_method": "published_test_sft_untouched_except_governance_filter",
        "quarantined_rows": len(quarantined),
        "quarantine_reasons": quarantine_counts,
        "split_sha256": split_hashes,
        "train_prompt_ids": sha256_text("\n".join(row["prompt_id"] for row in final_train_rows)),
        "validation_prompt_ids": sha256_text("\n".join(row["prompt_id"] for row in validation_rows)),
        "test_prompt_ids": sha256_text("\n".join(row["prompt_id"] for row in test_rows)),
        "heldout_used_for_training": False,
        "production_allowed": False,
        "attribution_required": False,
        "share_alike_required": False,
    }
    manifest["release_sha256"] = sha256_text(canonical_json(manifest))
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
