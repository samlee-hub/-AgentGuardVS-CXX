# AgentGuardVS-CXX Release Audit

- **Purpose**: produce a clean public release tree without development history,
  build intermediates, runtime run snapshots, local caches, or machine-specific paths,
  while keeping source, docs, tests, examples, scripts, agent skill/plugin assets,
  and minimal evaluation evidence.
- **Source repository**: AgentGuardVS-CXX (development repository)
- **Release output**: AgentGuardVS-CXX-RELEASE
- **Version**: 0.2.0
- **Generated**: 2026-06-06 02:48:49 +08:00

## Sizes

- Source repository (with history/build/runs): ~6.3 GB (build 5.6 GB, runs 516 MB, .vs 48 MB, .git 1.7 MB)
- Release tree: 2.10 MB
- Staged binary: 1.27 MB

## Exclusions applied

Excluded `.git`, `.vs`, `build`, `out`, `dist`, `runs` (incl. benchmark runs),
`vcpkg_installed`, `node_modules`, `__pycache__`, `.pytest_cache`, `docs/superpowers`,
and all build intermediates / archives by glob.

## Findings

- Absolute path / username leak: NONE (sanitized + verified)
- API key / token risk: NONE
- Sanitized files: benchmarks\AgentGuardBench\README.md, benchmarks\AgentGuardBench\configs\project_config.json, benchmarks\AgentGuardBench\reports\project_inventory.json, benchmarks\AgentGuardBench\reports\project_inventory.md, benchmarks\AgentGuardBench\reports\summary.html, benchmarks\AgentGuardBench\reports\summary.md, benchmarks\AgentGuardBench\reports\task_inventory.md, benchmarks\AgentGuardBench\tools\generate_readme.py, benchmarks\AgentGuardBench\tools\generate_report.py, docs\evaluation\blackbox-agent-test-v1\raw-test-results.md
- Debug binary shipped as release: no
- Release build completed: see TestSummary / report
- ctest completed: 100% tests passed, 0 failed out of 117 (Release, ctest 6.49s)

## Conclusion

PASS: release tree is clean (build/test status recorded above).
