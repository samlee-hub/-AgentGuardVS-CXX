#!/usr/bin/env python3
from __future__ import annotations

import argparse

from benchlib import markdown_list, read_json, resolve_run_dir, write_json, write_text


HALLUCINATION_KEYWORDS = [
    "undeclared identifier",
    "unresolved external symbol",
    "no member named",
    "is not a member of",
    "identifier not found",
]

CONFIG_EXTENSIONS = (".sln", ".vcxproj", ".Build.cs", ".filters")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Classify benchmark run failure.")
    parser.add_argument("--run", required=True)
    return parser.parse_args()


def load_optional(path, fallback):
    return read_json(path) if path.exists() else fallback


def classify(scope: dict, build: dict, semantic: dict) -> dict:
    evidence: list[str] = []
    modified = scope.get("modified_files", [])
    added = scope.get("added_files", [])
    allowed_count = max(1, len(scope.get("allowed_modified_files", [])) or 1)
    error_text = "\n".join(build.get("error_summary", [])).lower()

    if scope.get("violation_count", 0) > 0:
        evidence.append(f"scope violations={scope.get('violation_count')}")
        return result("SCOPE_VIOLATION", "Run modified files outside the declared task scope.", evidence, "Inspect scope_report.md and repair or discard out-of-scope edits.", 0.95)

    if not build.get("build_pass", False):
        if any(keyword in error_text for keyword in HALLUCINATION_KEYWORDS):
            evidence.extend(build.get("error_summary", [])[:8])
            return result("HALLUCINATED_API", "Build failed with errors indicating missing symbols, members, or unresolved APIs.", evidence, "Inspect build logs and replace invented APIs with existing project interfaces.", 0.85)
        if any(path.endswith(CONFIG_EXTENSIONS) for path in modified + added):
            evidence.append("Build failed after project/config files were modified.")
            return result("CONFIG_BREAKAGE", "Build configuration files changed and the build failed.", evidence, "Review project file changes first, then rerun build verification.", 0.8)
        evidence.append(build.get("failure_reason", "BUILD_FAILED"))
        evidence.extend(build.get("error_summary", [])[:8])
        return result("BUILD_FAILURE", "Build verification did not pass.", evidence, "Inspect build_result.json and build_logs.", 0.8)

    failed_checks = semantic.get("failed_checks", [])
    manual_checks = semantic.get("manual_review_checks", [])
    passed_count = sum(1 for item in semantic.get("checks", {}).values() if item.get("status") == "pass")

    if len(modified) > max(4, allowed_count * 2) or len(added) > 3:
        evidence.append(f"modified={len(modified)} added={len(added)} allowed_modified={allowed_count}")
        return result("OVER_REFACTORING", "Patch size is much larger than the allowed task surface.", evidence, "Reduce the patch to the smallest files needed for the task.", 0.75)

    if failed_checks and passed_count > 0:
        evidence.extend(failed_checks)
        return result("PARTIAL_COMPLETION", "Some semantic checks passed but others failed.", evidence, "Complete the missing semantic requirements and rerun verification.", 0.8)

    if failed_checks:
        evidence.extend(failed_checks)
        return result("SEMANTIC_MISMATCH", "Build passed but semantic checks failed.", evidence, "Compare the patch against task success criteria.", 0.85)

    if manual_checks:
        evidence.extend(manual_checks)
        return result("MANUAL_REVIEW_REQUIRED", "At least one semantic check could not be judged automatically.", evidence, "Review semantic_report.md manually.", 0.7)

    return result("SUCCESS", "Scope, build, and semantic checks all passed.", ["All automated checks passed."], "Record metrics and proceed to aggregate reporting.", 0.95)


def result(failure_type: str, reason: str, evidence: list[str], suggested_next_step: str, confidence: float) -> dict:
    return {
        "failure_type": failure_type,
        "reason": reason,
        "evidence": evidence,
        "suggested_next_step": suggested_next_step,
        "confidence": confidence,
    }


def main() -> int:
    args = parse_args()
    run_dir = resolve_run_dir(args.run)
    scope = load_optional(run_dir / "scope_result.json", {"scope_pass": False, "violation_count": 0, "modified_files": [], "added_files": []})
    build = load_optional(run_dir / "build_result.json", {"build_pass": False, "failure_reason": "BUILD_RESULT_MISSING", "error_summary": []})
    semantic = load_optional(run_dir / "semantic_result.json", {"semantic_pass": False, "failed_checks": [], "manual_review_checks": [], "checks": {}})
    classified = classify(scope, build, semantic)
    write_json(run_dir / "failure_result.json", classified)

    lines = [
        "# Failure Classification",
        "",
        f"- Failure type: `{classified['failure_type']}`",
        f"- Confidence: `{classified['confidence']}`",
        f"- Reason: {classified['reason']}",
        f"- Suggested next step: {classified['suggested_next_step']}",
        "",
        "## Evidence",
        markdown_list([str(item) for item in classified["evidence"]]),
        "",
    ]
    write_text(run_dir / "failure_report.md", "\n".join(lines))
    print(f"failure_type={classified['failure_type']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
