#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

from benchlib import (
    load_run_meta,
    load_task,
    markdown_list,
    normalize_rel,
    repo_dir,
    resolve_run_dir,
    scan_manifest,
    read_json,
    write_json,
    write_text,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check run modifications against task scope.")
    parser.add_argument("--run", required=True, help="Run directory, e.g. runs/T001/codex_raw")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    run_dir = resolve_run_dir(args.run)
    meta = load_run_meta(run_dir)
    task = load_task(meta["task_id"])
    baseline = read_json(run_dir / "baseline_manifest.json")["files"]
    current = scan_manifest(repo_dir(run_dir))

    baseline_paths = set(baseline)
    current_paths = set(current)
    modified_files = sorted(path for path in baseline_paths & current_paths if baseline[path]["sha256"] != current[path]["sha256"])
    added_files = sorted(current_paths - baseline_paths)
    deleted_files = sorted(baseline_paths - current_paths)
    unchanged_files_count = len(baseline_paths - set(modified_files) - set(deleted_files))

    allowed = {normalize_rel(path) for path in task.get("allowed_files", [])}
    forbidden = {normalize_rel(path) for path in task.get("forbidden_files", [])}
    allow_new_files = bool(task.get("allow_new_files", False))

    allowed_modified_files = sorted(path for path in modified_files if path in allowed)
    forbidden_modified_files = sorted(path for path in modified_files if path in forbidden)
    out_of_scope_modified_files = sorted(path for path in modified_files if path not in allowed and path not in forbidden)

    violations: list[dict[str, str]] = []
    for path in forbidden_modified_files:
        violations.append({"type": "forbidden_modified", "path": path, "reason": "File is listed in forbidden_files."})
    for path in out_of_scope_modified_files:
        violations.append({"type": "out_of_scope_modified", "path": path, "reason": "File is not listed in allowed_files."})
    for path in added_files:
        if allow_new_files:
            continue
        violations.append({"type": "added_file", "path": path, "reason": "New source/config files are not allowed by this task."})
    for path in deleted_files:
        violations.append({"type": "deleted_file", "path": path, "reason": "Deleting tracked source/config files is not allowed."})

    result = {
        "scope_pass": len(violations) == 0,
        "modified_files": modified_files,
        "added_files": added_files,
        "deleted_files": deleted_files,
        "unchanged_files_count": unchanged_files_count,
        "allowed_modified_files": allowed_modified_files,
        "forbidden_modified_files": forbidden_modified_files,
        "out_of_scope_modified_files": out_of_scope_modified_files,
        "violations": violations,
        "modified_files_count": len(modified_files),
        "added_files_count": len(added_files),
        "deleted_files_count": len(deleted_files),
        "violation_count": len(violations),
    }
    write_json(run_dir / "scope_result.json", result)

    report = [
        "# Scope Report",
        "",
        f"- Scope pass: `{str(result['scope_pass']).lower()}`",
        f"- Modified files: {len(modified_files)}",
        f"- Added files: {len(added_files)}",
        f"- Deleted files: {len(deleted_files)}",
        f"- Violations: {len(violations)}",
        "",
        "## Allowed Modified Files",
        markdown_list(allowed_modified_files),
        "",
        "## Forbidden Modified Files",
        markdown_list(forbidden_modified_files),
        "",
        "## Out-Of-Scope Modified Files",
        markdown_list(out_of_scope_modified_files),
        "",
        "## Added Files",
        markdown_list(added_files),
        "",
        "## Deleted Files",
        markdown_list(deleted_files),
        "",
        "## Violations",
    ]
    if violations:
        report.extend(f"- `{item['path']}`: {item['type']} - {item['reason']}" for item in violations)
    else:
        report.append("- None")
    write_text(run_dir / "scope_report.md", "\n".join(report) + "\n")
    print(f"scope_pass={result['scope_pass']} violations={len(violations)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
