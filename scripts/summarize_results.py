#!/usr/bin/env python3
"""Summarize AgentGuardVS-CXX benchmark outputs.

This helper is intentionally outside the core C++ execution path. It only reads
report.json and metrics.json files under a runs directory.
"""

from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
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


def error_counts_from_report(report: dict[str, Any]) -> Counter[str]:
    errors = report.get("build_result", {}).get("parsed_errors", [])
    counter: Counter[str] = Counter()
    for error in errors:
        category = error.get("category") or "Unknown"
        counter[str(category)] += 1
    return counter


def modified_files_from_report(report: dict[str, Any]) -> int:
    task_spec = report.get("task_spec", {})
    allowed_files = task_spec.get("allowed_files", [])
    return len(allowed_files) if isinstance(allowed_files, list) else 0


def summarize(runs: dict[str, dict[str, Any]]) -> dict[str, Any]:
    total_tasks = len(runs)
    successful_builds = 0
    repair_rounds_total = 0
    modified_files_total = 0
    error_type_counts: Counter[str] = Counter()

    for run in runs.values():
        report = run.get("report", {})
        metrics = run.get("metrics", {})

        build_success = metrics.get("build_success", report.get("success", False))
        if build_success:
            successful_builds += 1

        repair_rounds_total += int(metrics.get("repair_rounds", report.get("repair_rounds", 0)) or 0)
        modified_files_total += int(
            metrics.get("modified_file_count", modified_files_from_report(report)) or 0
        )

        metric_errors = metrics.get("error_type_counts")
        if isinstance(metric_errors, dict):
            for category, count in metric_errors.items():
                error_type_counts[str(category)] += int(count)
        else:
            error_type_counts.update(error_counts_from_report(report))

    success_rate = successful_builds / total_tasks if total_tasks else 0.0
    average_repair_rounds = repair_rounds_total / total_tasks if total_tasks else 0.0
    average_modified_files = modified_files_total / total_tasks if total_tasks else 0.0

    return {
        "total_tasks": total_tasks,
        "build_success_rate": success_rate,
        "average_repair_rounds": average_repair_rounds,
        "error_type_counts": dict(sorted(error_type_counts.items())),
        "average_modified_file_count": average_modified_files,
    }


def write_summary_csv(summary: dict[str, Any], output_path: Path) -> None:
    with output_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["metric", "value"])
        writer.writerow(["total_tasks", summary["total_tasks"]])
        writer.writerow(["build_success_rate", f"{summary['build_success_rate']:.4f}"])
        writer.writerow(["average_repair_rounds", f"{summary['average_repair_rounds']:.2f}"])
        writer.writerow(["average_modified_file_count", f"{summary['average_modified_file_count']:.2f}"])
        for category, count in summary["error_type_counts"].items():
            writer.writerow([f"error_type.{category}", count])


def write_summary_md(summary: dict[str, Any], output_path: Path) -> None:
    lines = [
        "# AgentGuardVS-CXX Benchmark Summary",
        "",
        "| Metric | Value |",
        "| --- | --- |",
        f"| Total tasks | {summary['total_tasks']} |",
        f"| Build success rate | {summary['build_success_rate']:.2%} |",
        f"| Average repair rounds | {summary['average_repair_rounds']:.2f} |",
        f"| Average modified file count | {summary['average_modified_file_count']:.2f} |",
        "",
        "## Error Type Distribution",
        "",
    ]

    if summary["error_type_counts"]:
        lines.extend(["| Error type | Count |", "| --- | --- |"])
        for category, count in summary["error_type_counts"].items():
            lines.append(f"| {category} | {count} |")
    else:
        lines.append("No parsed build errors were found.")

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize AgentGuardVS-CXX run results.")
    parser.add_argument("--runs-root", default="runs", help="Directory containing task run folders.")
    parser.add_argument("--output-dir", default=".", help="Directory for summary.csv and summary.md.")
    args = parser.parse_args()

    runs_root = Path(args.runs_root)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    summary = summarize(discover_runs(runs_root))
    write_summary_csv(summary, output_dir / "summary.csv")
    write_summary_md(summary, output_dir / "summary.md")

    print(f"Wrote {output_dir / 'summary.csv'}")
    print(f"Wrote {output_dir / 'summary.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
