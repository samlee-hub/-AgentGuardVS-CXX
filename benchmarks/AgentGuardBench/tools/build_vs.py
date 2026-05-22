#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path

from benchlib import load_run_meta, load_task, repo_dir, resolve_run_dir, write_json


ERROR_KEYWORDS = [
    "error C",
    "fatal error",
    "LNK",
    "MSB",
    "unresolved external symbol",
    "undeclared identifier",
    "no member named",
    "is not a member of",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build Visual Studio solutions for a run.")
    parser.add_argument("--run", required=True)
    return parser.parse_args()


def locate_msbuild() -> tuple[str, str]:
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if vswhere.exists():
        result = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.Component.MSBuild",
                "-find",
                r"MSBuild\**\Bin\MSBuild.exe",
            ],
            text=True,
            capture_output=True,
        )
        candidates = [line.strip() for line in result.stdout.splitlines() if line.strip()]
        if candidates:
            return candidates[0], "vswhere"
    found = shutil.which("msbuild")
    if found:
        return found, "where"
    return "", "not_found"


def extract_lines(text: str, keywords: list[str]) -> list[str]:
    results: list[str] = []
    lower_keywords = [keyword.lower() for keyword in keywords]
    for line in text.splitlines():
        lowered = line.lower()
        if any(keyword.lower() in lowered for keyword in lower_keywords):
            results.append(line.strip())
    return results


def discover_solutions(repo: Path, task: dict) -> list[Path]:
    configured = task.get("build_solutions") or []
    if configured:
        return [repo / item for item in configured]
    return sorted(repo.rglob("*.sln"))


def main() -> int:
    args = parse_args()
    run_dir = resolve_run_dir(args.run)
    meta = load_run_meta(run_dir)
    task = load_task(meta["task_id"])
    repo = repo_dir(run_dir)
    solutions = discover_solutions(repo, task)
    build_logs = run_dir / "build_logs"
    build_logs.mkdir(parents=True, exist_ok=True)

    msbuild_path, msbuild_source = locate_msbuild()
    result = {
        "build_pass": False,
        "failure_reason": "",
        "msbuild_path": msbuild_path,
        "msbuild_source": msbuild_source,
        "solutions": [str(path.relative_to(repo)).replace("\\", "/") if path.exists() else str(path) for path in solutions],
        "results": [],
        "error_summary": [],
        "warning_summary": [],
        "build_error_count": 0,
    }

    if not solutions:
        result["failure_reason"] = "NO_SOLUTION_FOUND"
        write_json(run_dir / "build_result.json", result)
        print("build_pass=false failure_reason=NO_SOLUTION_FOUND")
        return 0

    if not msbuild_path:
        result["failure_reason"] = "MSBUILD_NOT_FOUND"
        write_json(run_dir / "build_result.json", result)
        print("build_pass=false failure_reason=MSBUILD_NOT_FOUND")
        return 0

    all_passed = True
    for solution in solutions:
        solution_name = solution.stem if solution.exists() else Path(str(solution)).stem
        log_path = build_logs / f"{solution_name}.log"
        if not solution.exists():
            item = {
                "solution": str(solution),
                "build_pass": False,
                "return_code": -1,
                "log_path": str(log_path),
                "failure_reason": "SOLUTION_NOT_FOUND",
            }
            log_path.write_text("Solution not found.\n", encoding="utf-8")
            result["results"].append(item)
            all_passed = False
            continue

        command = [msbuild_path, str(solution), "/p:Configuration=Debug", "/m"]
        completed = subprocess.run(command, cwd=repo, text=True, capture_output=True)
        log_text = "Command: " + " ".join(command) + "\n\nSTDOUT:\n" + completed.stdout + "\nSTDERR:\n" + completed.stderr
        log_path.write_text(log_text, encoding="utf-8", errors="replace")
        errors = extract_lines(log_text, ERROR_KEYWORDS)
        warnings = extract_lines(log_text, ["warning C", "warning MSB", ": warning"])
        result["error_summary"].extend(errors)
        result["warning_summary"].extend(warnings)
        result["results"].append({
            "solution": str(solution.relative_to(repo)).replace("\\", "/"),
            "build_pass": completed.returncode == 0,
            "return_code": completed.returncode,
            "log_path": str(log_path),
            "command": command,
        })
        if completed.returncode != 0:
            all_passed = False

    result["build_pass"] = all_passed
    if not all_passed and not result["failure_reason"]:
        result["failure_reason"] = "BUILD_FAILED"
    result["build_error_count"] = len(result["error_summary"])
    write_json(run_dir / "build_result.json", result)
    print(f"build_pass={result['build_pass']} errors={result['build_error_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
