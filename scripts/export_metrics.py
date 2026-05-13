#!/usr/bin/env python3
"""Export per-task AgentGuardVS-CXX benchmark metrics.

This script is an offline helper. It only reads report.json and metrics.json
files under runs and writes derived tabular data for later analysis.
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def task_key(runs_root: Path, path: Path) -> str:
    try:
        relative = path.relative_to(runs_root)
    except ValueError:
        return path.parent.name
    return relative.parts[0] if relative.parts else path.parent.name


def discover_runs(runs_root: Path) -> dict[str, dict[str, Any]]:
    runs: dict[str, dict[str, Any]] = {}

    for report_path in runs_root.rglob("report.json"):
        key = task_key(runs_root, report_path)
        runs.setdefault(key, {})["report"] = load_json(report_path)

    for metrics_path in runs_root.rglob("metrics.json"):
        key = task_key(runs_root, metrics_path)
        runs.setdefault(key, {})["metrics"] = load_json(metrics_path)

    return runs


def count_report_errors(report: dict[str, Any]) -> int:
    errors = report.get("build_result", {}).get("parsed_errors", [])
    return len(errors) if isinstance(errors, list) else 0


def infer_modified_file_count(report: dict[str, Any]) -> int:
    allowed_files = report.get("task_spec", {}).get("allowed_files", [])
    return len(allowed_files) if isinstance(allowed_files, list) else 0


def build_rows(runs: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for key in sorted(runs):
        run = runs[key]
        report = run.get("report", {})
        metrics = run.get("metrics", {})
        task_spec = report.get("task_spec", {})

        rows.append({
            "run_id": key,
            "task_id": metrics.get("task_id", task_spec.get("task_id", key)),
            "build_success": metrics.get("build_success", report.get("success", False)),
            "repair_rounds": metrics.get("repair_rounds", report.get("repair_rounds", 0)),
            "modified_file_count": metrics.get(
                "modified_file_count",
                infer_modified_file_count(report),
            ),
            "parsed_error_count": count_report_errors(report),
            "error_type_counts": json.dumps(
                metrics.get("error_type_counts", {}),
                sort_keys=True,
                ensure_ascii=False,
            ),
        })
    return rows


def write_metrics_csv(rows: list[dict[str, Any]], output_path: Path) -> None:
    fieldnames = [
        "run_id",
        "task_id",
        "build_success",
        "repair_rounds",
        "modified_file_count",
        "parsed_error_count",
        "error_type_counts",
    ]
    with output_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description="Export AgentGuardVS-CXX per-task metrics.")
    parser.add_argument("--runs-root", default="runs", help="Directory containing task run folders.")
    parser.add_argument("--output-dir", default=".", help="Directory for metrics.csv and metrics.json.")
    args = parser.parse_args()

    runs_root = Path(args.runs_root)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    rows = build_rows(discover_runs(runs_root))
    write_metrics_csv(rows, output_dir / "metrics.csv")
    (output_dir / "metrics.json").write_text(
        json.dumps(rows, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    print(f"Wrote {output_dir / 'metrics.csv'}")
    print(f"Wrote {output_dir / 'metrics.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
