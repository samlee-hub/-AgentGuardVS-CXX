#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Callable

from benchlib import load_run_meta, load_task, markdown_list, read_text, repo_dir, resolve_run_dir, text_snippet, write_json, write_text


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run semantic checks for a benchmark run.")
    parser.add_argument("--run", required=True)
    return parser.parse_args()


def read_repo_files(repo: Path, paths: list[str]) -> dict[str, str]:
    result: dict[str, str] = {}
    for rel in paths:
        path = repo / rel
        if path.exists() and path.is_file():
            result[rel] = read_text(path)
    return result


def all_text(files: dict[str, str]) -> str:
    return "\n".join(files.values())


def status(pass_condition: bool, evidence: list[str], manual: bool = False) -> dict:
    if manual:
        return {"status": "manual_review_required", "evidence": evidence}
    return {"status": "pass" if pass_condition else "fail", "evidence": evidence}


def contains_any(text: str, patterns: list[str]) -> bool:
    lower = text.lower()
    return any(pattern.lower() in lower for pattern in patterns)


def state_contains_unit_count(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    evidence = []
    for rel, content in files.items():
        if "MessageTypes::State" in content and contains_any(content, ["unit_count", "unitcount", "unit count", "CountUnits", ".size()"]):
            evidence.append(f"{rel}: {text_snippet(content, 'MessageTypes::State')}")
    return status(bool(evidence), evidence or ["No STATE unit-count field or equivalent count logic found in allowed files."])


def summon_global_cooldown_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["actionCooldown", "globalCooldown", "summonCooldown"]),
        contains_any(text, ["> 0.0", "> 0", "remaining"]),
        contains_any(text, ["SUMMON_COOLDOWN", "COOLDOWN"]),
        contains_any(text, ["= 3.0", "chrono", "seconds"]),
    ]
    evidence = [f"cooldown evidence in allowed files: {sum(1 for item in checks if item)}/4"]
    return status(all(checks), evidence)


def insufficient_mana_error_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["resource < unit.GetCost", "mana <"]),
        contains_any(text, ["MessageTypes::Error"]),
        contains_any(text, ["NOT_ENOUGH_MANA", "INSUFFICIENT_MANA"]),
    ]
    return status(all(checks), [f"insufficient mana evidence in allowed files: {sum(1 for item in checks if item)}/3"])


def mmr_closest_score_logic_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["score"]),
        contains_any(text, ["abs(", "std::abs", "diff"]),
        contains_any(text, ["sort(", "min", "closest"]),
        contains_any(text, ["leftDiff", "rightDiff", "anchorScore"]),
    ]
    return status(all(checks), [f"closest-score evidence in allowed files: {sum(1 for item in checks if item)}/4"])


def factory_invalid_mode_guard_exists(repo: Path, task: dict) -> dict:
    paths = task["allowed_files"] + ["Server1/Server1/Server/MatchManager.cpp"]
    files = read_repo_files(repo, paths)
    text = all_text(files)
    checks = [
        contains_any(text, ["FindDescriptor"]),
        contains_any(text, ["descriptor == nullptr", "nullptr"]),
        contains_any(text, ["return nullptr", "UNSUPPORTED_MODE", "MODE_CREATE_FAILED"]),
    ]
    return status(all(checks), [f"invalid mode guard evidence: {sum(1 for item in checks if item)}/3"])


def tick_dt_clamp_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["Tick(double dt)", "UpdateLoop"]),
        contains_any(text, ["clamp", "maxDt", "min(", "max(", "dt < 0", "dt >"]),
    ]
    return status(all(checks), [f"dt clamp evidence in allowed files: {sum(1 for item in checks if item)}/2"])


def client_state_missing_fields_guard_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["MessageTypes::State"]),
        contains_any(text, ["msg.params.size() >=", "params.size() >=", ".size() >"]),
        contains_any(text, ["格式错误", "format", "missing", "default"]),
    ]
    return status(all(checks), [f"client STATE missing-field evidence: {sum(1 for item in checks if item)}/3"])


def message_parser_edge_case_guard_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["Parse(const string& text)"]),
        contains_any(text, ["text.empty()", "empty()", "PARAM_ERROR", "malformed"]),
        contains_any(text, ["||", "empty parameter", "trim", "continue"]),
    ]
    return status(all(checks), [f"message parser edge-case evidence: {sum(1 for item in checks if item)}/3"])


def duplicate_login_guard_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["m_onlineUsers.find", "IsUserOnline"]),
        contains_any(text, ["USER_ONLINE", "ALREADY_LOGIN"]),
        contains_any(text, ["m_onlineUsers.insert", "currentUser"]),
    ]
    return status(all(checks), [f"duplicate login evidence: {sum(1 for item in checks if item)}/3"])


def duplicate_match_queue_guard_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["FindWaitingPlayerIndex"]),
        contains_any(text, ["waitingIndex != -1", "ALREADY"]),
        contains_any(text, ["HasPlayer", "ALREADY_IN_ROOM"]),
    ]
    return status(all(checks), [f"duplicate match queue evidence: {sum(1 for item in checks if item)}/3"])


def battle_result_once_guard_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["m_recordWritten"]),
        contains_any(text, ["m_resultUpdated"]),
        len(re.findall(r"if\s*\(\s*m_(?:recordWritten|resultUpdated)\s*\)", text)) >= 2,
    ]
    return status(all(checks), [f"battle result once evidence: {sum(1 for item in checks if item)}/3"])


def malformed_state_file_guard_exists(repo: Path, task: dict) -> dict:
    files = read_repo_files(repo, task["allowed_files"])
    text = all_text(files)
    checks = [
        contains_any(text, ["ParseIntoArray", "BattleState.txt", "UESTATE"]),
        contains_any(text, ["Params.Num() <", "Fields.Num() <"]),
        contains_any(text, ["continue", "return"]),
    ]
    return status(all(checks), [f"malformed state evidence: {sum(1 for item in checks if item)}/3"])


CHECKS: dict[str, Callable[[Path, dict], dict]] = {
    "state_contains_unit_count": state_contains_unit_count,
    "summon_global_cooldown_exists": summon_global_cooldown_exists,
    "insufficient_mana_error_exists": insufficient_mana_error_exists,
    "mmr_closest_score_logic_exists": mmr_closest_score_logic_exists,
    "factory_invalid_mode_guard_exists": factory_invalid_mode_guard_exists,
    "tick_dt_clamp_exists": tick_dt_clamp_exists,
    "client_state_missing_fields_guard_exists": client_state_missing_fields_guard_exists,
    "message_parser_edge_case_guard_exists": message_parser_edge_case_guard_exists,
    "duplicate_login_guard_exists": duplicate_login_guard_exists,
    "duplicate_match_queue_guard_exists": duplicate_match_queue_guard_exists,
    "battle_result_once_guard_exists": battle_result_once_guard_exists,
    "malformed_state_file_guard_exists": malformed_state_file_guard_exists,
}


def main() -> int:
    args = parse_args()
    run_dir = resolve_run_dir(args.run)
    meta = load_run_meta(run_dir)
    task = load_task(meta["task_id"])
    repo = repo_dir(run_dir)

    checks: dict[str, dict] = {}
    for check_name in task.get("semantic_checks", []):
        fn = CHECKS.get(check_name)
        if fn is None:
            checks[check_name] = status(False, [f"Unknown semantic check: {check_name}"], manual=True)
            continue
        checks[check_name] = fn(repo, task)

    failed = sorted(name for name, item in checks.items() if item["status"] == "fail")
    manual = sorted(name for name, item in checks.items() if item["status"] == "manual_review_required")
    result = {
        "semantic_pass": not failed and not manual,
        "checks": checks,
        "failed_checks": failed,
        "manual_review_checks": manual,
        "evidence": {name: item.get("evidence", []) for name, item in checks.items()},
    }
    write_json(run_dir / "semantic_result.json", result)

    lines = [
        "# Semantic Report",
        "",
        f"- Semantic pass: `{str(result['semantic_pass']).lower()}`",
        f"- Failed checks: {len(failed)}",
        f"- Manual review checks: {len(manual)}",
        "",
        "## Failed Checks",
        markdown_list(failed),
        "",
        "## Manual Review Checks",
        markdown_list(manual),
        "",
        "## Evidence",
    ]
    for name, item in checks.items():
        lines.append(f"### {name}")
        lines.append(f"- Status: `{item['status']}`")
        for evidence in item.get("evidence", []):
            lines.append(f"- {evidence}")
        lines.append("")
    write_text(run_dir / "semantic_report.md", "\n".join(lines))
    print(f"semantic_pass={result['semantic_pass']} failed={len(failed)} manual={len(manual)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
