#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from benchlib import BENCH_ROOT


def benchmark_results() -> str:
    summary = BENCH_ROOT / "reports" / "summary.md"
    if not summary.exists():
        return "Not generated yet."
    text = summary.read_text(encoding="utf-8")
    marker = "## Raw vs Guarded Summary"
    index = text.find(marker)
    if index < 0:
        return "Not generated yet."
    next_index = text.find("\n## ", index + len(marker))
    return text[index: next_index if next_index > index else len(text)].strip()


def main() -> int:
    readme = f"""# AgentGuard Bench: Reliability Evaluation for AI Coding Agents on a Real C++ Game Architecture

## Problem

AI coding agents can modify unrelated files, generate non-compiling patches, over-refactor existing logic, and leave developers without reproducible audit traces.

AgentGuard Bench turns a real Visual Studio C++ multiplayer room battle game architecture into a reproducible reliability evaluation system for comparing raw agent execution with guarded agent execution.

## Why This Testbed

The benchmark uses `D:\\testrepo\\frame`, a real project copy containing server, client, and UE-facing state visualization code. The codebase includes protocol parsing, matchmaking, user/session state, battle simulation, result persistence, and state rendering paths, which are common failure points for coding agents.

## Architecture

```text
TaskSpec
-> Run Preparation
-> Isolated Repo Copy
-> Raw/Guarded Agent Execution
-> Scope Check
-> MSBuild Verification
-> Semantic Check
-> Failure Classification
-> Report
```

## TaskSpec

Each task under `tasks/<task_id>/task.json` defines the task goal, `allowed_files`, `forbidden_files`, semantic checks, success criteria, risk points, and status. Paths are relative to the source project root and are validated against the scanned project inventory.

## Raw vs Guarded Execution

`prompt_raw.md` simulates ordinary Codex usage: it describes the user goal and lets the agent decide which files to edit.

`prompt_guarded.md` simulates controlled execution: it lists allowed and forbidden files, requires a PatchPlan, and instructs the agent to stop instead of editing outside scope.

## Scope Checking

`tools/check_scope.py` compares the run repository against `baseline_manifest.json`. It reports modified, added, and deleted files, then classifies changes as allowed, forbidden, or out of scope.

## Build Verification

`tools/build_vs.py` looks for `.sln` files and invokes MSBuild through `vswhere` or `where msbuild`. If no solution is present, it records `NO_SOLUTION_FOUND` instead of inventing a build target.

## Semantic Checks

`tools/semantic_check.py` runs task-specific source-text oracles for protocol fields, cooldowns, matchmaking, parser hardening, duplicate guards, battle result idempotency, and malformed state handling. Ambiguous checks can be marked for manual review.

## Failure Taxonomy

`tools/classify_failure.py` classifies runs as `SUCCESS`, `SCOPE_VIOLATION`, `BUILD_FAILURE`, `HALLUCINATED_API`, `SEMANTIC_MISMATCH`, `PARTIAL_COMPLETION`, `OVER_REFACTORING`, `CONFIG_BREAKAGE`, `MANUAL_REVIEW_REQUIRED`, or `UNKNOWN_FAILURE`.

## Benchmark Results

{benchmark_results()}

## How to Reproduce

```bat
cd /d D:\\testrepo\\AgentGuardBench
scripts\\prepare_all.bat
scripts\\verify_all.bat
scripts\\report_all.bat
```

For a single task, run Codex inside one prepared repo, apply the prompt from that run directory, then verify:

```bat
cd /d D:\\testrepo\\AgentGuardBench
python tools\\verify_run.py --run runs\\T001_state_unit_count\\codex_raw
```

## Limitations

The current first-version benchmark is based on one real project copy, `D:\\testrepo\\frame`. Although the repository count is limited, the task set covers multiple modules and validation dimensions that reflect common coding-agent failure modes in real engineering work.

Current semantic checks are heuristic and source-text based. Build verification currently depends on `.sln` discovery, while this project copy exposes `.vcxproj` files but no scanned `.sln`.

## Roadmap

- add more repositories
- add CI integration
- add richer semantic oracles
- add agent trace recorder
- add cross-agent comparison
"""
    (BENCH_ROOT / "README.md").write_text(readme, encoding="utf-8")
    print(f"Wrote {BENCH_ROOT / 'README.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
