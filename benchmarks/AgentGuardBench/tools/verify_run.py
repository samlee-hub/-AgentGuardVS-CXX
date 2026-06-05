#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from benchlib import BENCH_ROOT, load_run_meta, markdown_list, read_json, resolve_run_dir, write_json, write_text


STEPS = [
    "check_scope.py",
    "build_vs.py",
    "semantic_check.py",
    "classify_failure.py",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run all verification steps for one benchmark run.")
    parser.add_argument("--run", required=True)
    return parser.parse_args()


def run_step(script_name: str, run_dir: Path) -> dict:
    completed = subprocess.run(
        [sys.executable, str(BENCH_ROOT / "tools" / script_name), "--run", str(run_dir)],
        cwd=BENCH_ROOT,
        text=True,
        capture_output=True,
    )
    return {
        "script": script_name,
        "return_code": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
    }


def load_optional(path: Path, fallback: dict) -> dict:
    return read_json(path) if path.exists() else fallback


def main() -> int:
    args = parse_args()
    run_dir = resolve_run_dir(args.run)
    meta = load_run_meta(run_dir)
    step_results = [run_step(script, run_dir) for script in STEPS]

    scope = load_optional(run_dir / "scope_result.json", {})
    build = load_optional(run_dir / "build_result.json", {})
    semantic = load_optional(run_dir / "semantic_result.json", {})
    failure = load_optional(run_dir / "failure_result.json", {"failure_type": "UNKNOWN_FAILURE"})

    verify_result = {
        "task_id": meta.get("task_id", ""),
        "mode": meta.get("mode", ""),
        "task_title": meta.get("task_title", ""),
        "scope_pass": bool(scope.get("scope_pass", False)),
        "build_pass": bool(build.get("build_pass", False)),
        "semantic_pass": bool(semantic.get("semantic_pass", False)),
        "final_success": bool(scope.get("scope_pass", False)) and bool(build.get("build_pass", False)) and bool(semantic.get("semantic_pass", False)) and failure.get("failure_type") == "SUCCESS",
        "failure_type": failure.get("failure_type", "UNKNOWN_FAILURE"),
        "modified_files_count": int(scope.get("modified_files_count", 0) or 0),
        "added_files_count": int(scope.get("added_files_count", 0) or 0),
        "deleted_files_count": int(scope.get("deleted_files_count", 0) or 0),
        "violation_count": int(scope.get("violation_count", 0) or 0),
        "build_error_count": int(build.get("build_error_count", 0) or 0),
        "failed_semantic_checks": semantic.get("failed_checks", []),
        "manual_review_checks": semantic.get("manual_review_checks", []),
        "steps": step_results,
    }
    write_json(run_dir / "verify_result.json", verify_result)

    lines = [
        "# Run Verification Report",
        "",
        f"- Task: `{verify_result['task_id']}` - {verify_result['task_title']}",
        f"- Mode: `{verify_result['mode']}`",
        f"- Final success: `{str(verify_result['final_success']).lower()}`",
        f"- Failure type: `{verify_result['failure_type']}`",
        "",
        "## Modified Files",
        markdown_list(scope.get("modified_files", [])),
        "",
        "## Scope Check",
        f"- Scope pass: `{str(verify_result['scope_pass']).lower()}`",
        f"- Violations: {verify_result['violation_count']}",
        "",
        "## Build Result",
        f"- Build pass: `{str(verify_result['build_pass']).lower()}`",
        f"- Build errors: {verify_result['build_error_count']}",
        f"- Failure reason: `{build.get('failure_reason', '')}`",
        "",
        "## Semantic Checks",
        f"- Semantic pass: `{str(verify_result['semantic_pass']).lower()}`",
        "### Failed",
        markdown_list(verify_result["failed_semantic_checks"]),
        "",
        "### Manual Review",
        markdown_list(verify_result["manual_review_checks"]),
        "",
        "## Failure Classification",
        f"- Reason: {failure.get('reason', '')}",
        f"- Suggested next step: {failure.get('suggested_next_step', '')}",
        "",
        "## Step Execution",
    ]
    for step in step_results:
        lines.append(f"- `{step['script']}` return_code={step['return_code']}")
    write_text(run_dir / "report.md", "\n".join(lines) + "\n")
    print(f"final_success={verify_result['final_success']} failure_type={verify_result['failure_type']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
