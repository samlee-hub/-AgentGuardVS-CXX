#!/usr/bin/env python3
from __future__ import annotations

import csv
import html
from collections import Counter, defaultdict
from pathlib import Path

from benchlib import BENCH_ROOT


def read_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def bool_value(value: str) -> bool:
    return str(value).strip().lower() == "true"


def rate(rows: list[dict[str, str]], field: str, expected: bool = True) -> float:
    if not rows:
        return 0.0
    count = sum(1 for row in rows if bool_value(row.get(field, "")) is expected)
    return count / len(rows)


def pct(value: float) -> str:
    return f"{value:.2%}"


def mode_rows(rows: list[dict[str, str]], mode: str) -> list[dict[str, str]]:
    return [row for row in rows if row.get("mode") == mode]


def split_semicolon(value: str) -> list[str]:
    return [item for item in value.split(";") if item]


def generated_baseline_note(rows: list[dict[str, str]]) -> str:
    if not rows:
        return "No verification rows are available. This is not enough data for benchmark conclusions."
    all_unmodified = all(int(row.get("modified_files_count", "0") or 0) == 0 for row in rows)
    if all_unmodified:
        return "All current rows have zero modified files, so this report reflects prepared-run baseline verification rather than completed raw-vs-guarded agent executions."
    return "Rows include modified runs and can be used as first-pass benchmark evidence."


def build_summary_markdown(rows: list[dict[str, str]]) -> str:
    raw = mode_rows(rows, "codex_raw")
    guarded = mode_rows(rows, "codex_guarded")
    failure_counts = Counter(row.get("failure_type", "UNKNOWN_FAILURE") for row in rows)
    by_task: dict[str, dict[str, dict[str, str]]] = defaultdict(dict)
    for row in rows:
        by_task[row["task_id"]][row["mode"]] = row

    lines = [
        "# AgentGuard Bench Summary",
        "",
        "## Raw vs Guarded Summary",
        "",
        generated_baseline_note(rows),
        "",
        "| Metric | Value |",
        "| --- | ---: |",
        f"| Total verification rows | {len(rows)} |",
        f"| codex_raw run count | {len(raw)} |",
        f"| codex_guarded run count | {len(guarded)} |",
        f"| Raw success rate | {pct(rate(raw, 'final_success'))} |",
        f"| Guarded success rate | {pct(rate(guarded, 'final_success'))} |",
        f"| Raw build pass rate | {pct(rate(raw, 'build_pass'))} |",
        f"| Guarded build pass rate | {pct(rate(guarded, 'build_pass'))} |",
        f"| Raw scope violation rate | {pct(rate(raw, 'scope_pass', expected=False))} |",
        f"| Guarded scope violation rate | {pct(rate(guarded, 'scope_pass', expected=False))} |",
        f"| Raw semantic pass rate | {pct(rate(raw, 'semantic_pass'))} |",
        f"| Guarded semantic pass rate | {pct(rate(guarded, 'semantic_pass'))} |",
        "",
        "## failure_type Distribution",
        "",
    ]
    if failure_counts:
        lines.extend(["| failure_type | Count |", "| --- | ---: |"])
        for failure_type, count in sorted(failure_counts.items()):
            lines.append(f"| `{failure_type}` | {count} |")
    else:
        lines.append("No failure data available.")

    lines.extend([
        "",
        "## Raw vs Guarded By Task",
        "",
        "| Task | Raw final_success | Raw failure_type | Guarded final_success | Guarded failure_type |",
        "| --- | --- | --- | --- | --- |",
    ])
    for task_id in sorted(by_task):
        raw_row = by_task[task_id].get("codex_raw", {})
        guarded_row = by_task[task_id].get("codex_guarded", {})
        lines.append(
            f"| `{task_id}` | {raw_row.get('final_success', 'missing')} | `{raw_row.get('failure_type', 'missing')}` | "
            f"{guarded_row.get('final_success', 'missing')} | `{guarded_row.get('failure_type', 'missing')}` |"
        )

    lines.extend([
        "",
        "## Typical Failure Cases",
        "",
    ])
    typical = [row for row in rows if row.get("failure_type") != "SUCCESS"][:5]
    if typical:
        for row in typical:
            failed_checks = split_semicolon(row.get("failed_semantic_checks", ""))
            manual_checks = split_semicolon(row.get("manual_review_checks", ""))
            lines.append(
                f"- `{row['task_id']}` / `{row['mode']}`: `{row.get('failure_type')}`, "
                f"build_pass={row.get('build_pass')}, scope_pass={row.get('scope_pass')}, "
                f"semantic_pass={row.get('semantic_pass')}, failed_checks={failed_checks}, manual_review={manual_checks}"
            )
    else:
        lines.append("- No failures recorded.")

    lines.extend([
        "",
        "## Current Limitations",
        "",
        "- If all rows have zero modified files, the report is only a baseline verification of prepared workspaces.",
        "- `D:\\testrepo\\frame` currently has `.vcxproj` files but no `.sln`, so MSBuild solution verification reports `NO_SOLUTION_FOUND` until project-level build support or solution generation is added.",
        "- Semantic checks are first-version source-text heuristics and should be treated as benchmark oracles with manual review for ambiguous cases.",
        "- Data is from one real project copy, so conclusions are internal first-version evidence rather than a broad model leaderboard.",
        "",
    ])
    return "\n".join(lines)


def markdown_to_simple_html(markdown: str) -> str:
    escaped = html.escape(markdown)
    return (
        "<!doctype html>\n<html><head><meta charset=\"utf-8\">"
        "<title>AgentGuard Bench Summary</title>"
        "<style>body{font-family:Segoe UI,Arial,sans-serif;max-width:1100px;margin:32px auto;line-height:1.5}"
        "pre{white-space:pre-wrap;background:#f6f8fa;padding:16px;border:1px solid #ddd}</style>"
        "</head><body><pre>"
        + escaped
        + "</pre></body></html>\n"
    )


def main() -> int:
    reports = BENCH_ROOT / "reports"
    rows = read_rows(reports / "metrics.csv")
    summary = build_summary_markdown(rows)
    (reports / "summary.md").write_text(summary, encoding="utf-8")
    (reports / "summary.html").write_text(markdown_to_simple_html(summary), encoding="utf-8")
    print(f"Wrote {reports / 'summary.md'}")
    print(f"Wrote {reports / 'summary.html'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
