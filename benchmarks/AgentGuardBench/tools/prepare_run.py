#!/usr/bin/env python3
from __future__ import annotations

import argparse
import fnmatch
import shutil
from pathlib import Path

from benchlib import BENCH_ROOT, load_config, load_task, now_utc, scan_manifest, write_json, write_text


EXCLUDE_DIRS = {
    ".git",
    ".vs",
    "x64",
    "Debug",
    "Release",
    "Binaries",
    "Intermediate",
    "Saved",
    "DerivedDataCache",
    "__pycache__",
    ".idea",
    ".vscode",
    "AgentGuardBench",
}

EXCLUDE_FILE_PATTERNS = {
    "*.sdf",
    "*.ipch",
    "*.pdb",
    "*.obj",
    "*.exe",
    "*.ilk",
    "*.log",
}

VALID_MODES = {"codex_raw", "codex_guarded"}


def ignore_names(_directory: str, names: list[str]) -> set[str]:
    ignored: set[str] = set()
    for name in names:
        if name in EXCLUDE_DIRS:
            ignored.add(name)
            continue
        if any(fnmatch.fnmatch(name, pattern) for pattern in EXCLUDE_FILE_PATTERNS):
            ignored.add(name)
    return ignored


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Prepare an isolated benchmark run repository.")
    parser.add_argument("--task", required=True, help="Task id, e.g. T001_state_unit_count")
    parser.add_argument("--mode", required=True, choices=sorted(VALID_MODES))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config = load_config()
    source_repo = Path(config["source_repo"]).resolve()
    task = load_task(args.task)

    if not source_repo.exists() or not source_repo.is_dir():
        raise SystemExit(f"source_repo does not exist: {source_repo}")

    run_dir = BENCH_ROOT / "runs" / args.task / args.mode
    repo = run_dir / "repo"
    log_lines: list[str] = []

    if run_dir.exists():
        log_lines.append(f"{now_utc()} removing existing run directory: {run_dir}")
        shutil.rmtree(run_dir)

    run_dir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source_repo, repo, ignore=ignore_names)

    prompt_type = "prompt_raw.md" if args.mode == "codex_raw" else "prompt_guarded.md"
    prompt_source = BENCH_ROOT / "tasks" / args.task / prompt_type
    if not prompt_source.exists():
        raise SystemExit(f"prompt file missing: {prompt_source}")
    shutil.copyfile(prompt_source, run_dir / "prompt.md")

    manifest = {
        "schema_version": "1.0",
        "generated_at": now_utc(),
        "repo": str(repo),
        "files": scan_manifest(repo),
    }
    write_json(run_dir / "baseline_manifest.json", manifest)

    run_meta = {
        "task_id": args.task,
        "mode": args.mode,
        "source_repo": str(source_repo),
        "run_dir": str(run_dir),
        "created_at": now_utc(),
        "prompt_type": prompt_type,
        "task_title": task["title"],
        "status": "prepared",
    }
    write_json(run_dir / "run_meta.json", run_meta)
    if log_lines:
        write_text(run_dir / "prepare.log", "\n".join(log_lines) + "\n")

    print(f"Prepared {run_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
