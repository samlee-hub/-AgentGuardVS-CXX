#!/usr/bin/env python3
from __future__ import annotations

import csv
import subprocess
import sys
from pathlib import Path

from benchlib import BENCH_ROOT, read_json


FIELDNAMES = [
    "task_id",
    "mode",
    "scope_pass",
    "build_pass",
    "semantic_pass",
    "final_success",
    "failure_type",
    "modified_files_count",
    "added_files_count",
    "deleted_files_count",
    "violation_count",
    "build_error_count",
    "failed_semantic_checks",
    "manual_review_checks",
]


def discover_run_dirs() -> list[Path]:
    runs_root = BENCH_ROOT / "runs"
    if not runs_root.exists():
        return []
    return sorted(path for path in runs_root.glob("T*/codex_*") if (path / "run_meta.json").exists())


def run_verify(run_dir: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(BENCH_ROOT / "tools" / "verify_run.py"), "--run", str(run_dir)],
        cwd=BENCH_ROOT,
        text=True,
        capture_output=True,
    )


def as_csv_row(result: dict) -> dict[str, str]:
    row = {}
    for field in FIELDNAMES:
        value = result.get(field, "")
        if isinstance(value, list):
            row[field] = ";".join(str(item) for item in value)
        else:
            row[field] = str(value).lower() if isinstance(value, bool) else str(value)
    return row


def main() -> int:
    reports_dir = BENCH_ROOT / "reports"
    reports_dir.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, str]] = []
    failures: list[str] = []

    for run_dir in discover_run_dirs():
        completed = run_verify(run_dir)
        if completed.returncode != 0:
            failures.append(f"{run_dir}: {completed.stderr.strip() or completed.stdout.strip()}")
        verify_path = run_dir / "verify_result.json"
        if verify_path.exists():
            rows.append(as_csv_row(read_json(verify_path)))
        else:
            failures.append(f"{run_dir}: verify_result.json missing")

    metrics_path = reports_dir / "metrics.csv"
    with metrics_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDNAMES)
        writer.writeheader()
        writer.writerows(rows)

    if failures:
        (reports_dir / "verify_all_errors.log").write_text("\n".join(failures) + "\n", encoding="utf-8")

    print(f"Wrote {metrics_path} rows={len(rows)} failures={len(failures)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
