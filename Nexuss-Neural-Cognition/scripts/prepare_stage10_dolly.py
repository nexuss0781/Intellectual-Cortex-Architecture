#!/usr/bin/env python3
"""Prepare the licensed public Dolly source into an auditable Stage 10 SFT release."""
from __future__ import annotations

import collections
import hashlib
import json
import re
import sys
from pathlib import Path

SOURCE = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("data/stage10-hf/databricks-dolly-15k.jsonl")
OUT = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("data/stage10-hf/derived")
OUT.mkdir(parents=True, exist_ok=True)

EMAIL = re.compile(r"\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b", re.I)
PHONE = re.compile(r"(?<!\d)(?:\+?\d[\d ()-]{8,}\d)(?!\d)")
SECRET = re.compile(r"(?:api[_ -]?key|secret|password|private key|BEGIN [A-Z ]+ KEY|aws_access_key|bearer token)", re.I)

rows = []
quarantined = []
seen = set()
category_counts = collections.Counter()
reason_counts = collections.Counter()

with SOURCE.open(encoding="utf-8") as handle:
    for line_number, line in enumerate(handle, 1):
        try:
            raw = json.loads(line)
        except json.JSONDecodeError as exc:
            quarantined.append({"line": line_number, "reason": "malformed_json", "detail": str(exc)})
            reason_counts["malformed_json"] += 1
            continue
        required = ("instruction", "context", "response", "category")
        if any(not isinstance(raw.get(field), str) for field in required):
            quarantined.append({"line": line_number, "reason": "schema_or_type", "record": raw})
            reason_counts["schema_or_type"] += 1
            continue
        canonical = "\x1f".join(raw[field].strip() for field in required)
        row_hash = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
        if row_hash in seen:
            quarantined.append({"line": line_number, "reason": "exact_duplicate", "row_hash": row_hash})
            reason_counts["exact_duplicate"] += 1
            continue
        seen.add(row_hash)
        text = "\n".join(raw[field] for field in ("instruction", "context", "response"))
        reasons = []
        if EMAIL.search(text):
            reasons.append("email_pattern")
        if PHONE.search(text):
            reasons.append("phone_pattern")
        if SECRET.search(text):
            reasons.append("secret_pattern")
        if not raw["instruction"].strip() or not raw["response"].strip():
            reasons.append("empty_required_text")
        if len(raw["instruction"]) > 12000 or len(raw["context"]) > 24000 or len(raw["response"]) > 12000:
            reasons.append("length_limit")
        if reasons:
            quarantined.append({"line": line_number, "reason": "+".join(reasons), "row_hash": row_hash, "category": raw["category"]})
            for reason in reasons:
                reason_counts[reason] += 1
            continue
        row = {
            "row_hash": row_hash,
            "instruction": raw["instruction"].strip(),
            "context": raw["context"].strip(),
            "response": raw["response"].strip(),
            "category": raw["category"].strip(),
            "source": "databricks/databricks-dolly-15k",
            "source_license": "cc-by-sa-3.0",
            "production_allowed": False,
            "pilot_training_allowed": True,
        }
        rows.append(row)
        category_counts[row["category"]] += 1

rows.sort(key=lambda row: row["row_hash"])
count = len(rows)
train_end = (count * 80) // 100
dev_end = (count * 90) // 100
splits = {
    "train": rows[:train_end],
    "development": rows[train_end:dev_end],
    "heldout": rows[dev_end:],
}

for split, split_rows in splits.items():
    with (OUT / f"{split}.jsonl").open("w", encoding="utf-8") as handle_json, (OUT / f"{split}.tsv").open("w", encoding="utf-8") as handle_tsv:
        handle_tsv.write("row_hash\tinstruction\tcontext\tresponse\tcategory\tsource_license\tproduction_allowed\tpilot_training_allowed\n")
        for row in split_rows:
            handle_json.write(json.dumps({**row, "split": split}, ensure_ascii=False, sort_keys=True) + "\n")
            safe_fields = [row["row_hash"], row["instruction"], row["context"], row["response"], row["category"], row["source_license"], "0", "1"]
            handle_tsv.write("\t".join(field.replace("\t", " ").replace("\n", " ") for field in safe_fields) + "\n")

with (OUT / "quarantine.jsonl").open("w", encoding="utf-8") as handle:
    for item in quarantined:
        handle.write(json.dumps(item, ensure_ascii=False, sort_keys=True) + "\n")

source_hash = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
release_canonical = "\n".join(row["row_hash"] for row in rows) + "\n"
release_hash = hashlib.sha256(release_canonical.encode("utf-8")).hexdigest()
manifest = {
    "dataset_id": "databricks/databricks-dolly-15k",
    "dataset_url": "https://huggingface.co/datasets/databricks/databricks-dolly-15k",
    "raw_file": SOURCE.name,
    "raw_sha256": source_hash,
    "declared_license": "cc-by-sa-3.0",
    "attribution_required": True,
    "share_alike_required": True,
    "source_rows": 15000,
    "valid_unique_rows": len(rows),
    "quarantined_rows": len(quarantined),
    "split_counts": {split: len(split_rows) for split, split_rows in splits.items()},
    "release_sha256": release_hash,
    "categories": dict(sorted(category_counts.items())),
    "quarantine_reasons": dict(sorted(reason_counts.items())),
    "languages": ["en"],
    "production_allowed": False,
    "pilot_training_allowed": True,
    "hidden_heldout_for_training": True,
    "split_method": "sort_by_row_sha256_then_80_10_10",
    "source_lineage": "Every retained row carries source and row_hash fields.",
}
(OUT / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
(OUT / "LICENSE_NOTICE.md").write_text(
    "# Derived Release License Notice\n\n"
    "Source: `databricks/databricks-dolly-15k`\n"
    "Declared source license: **CC BY-SA 3.0**\n\n"
    "This derived pilot release preserves attribution and share-alike obligations. "
    "It is approved for this bounded engineering pilot only and is not production-release authorization.\n",
    encoding="utf-8",
)
print(json.dumps(manifest, indent=2, sort_keys=True))
