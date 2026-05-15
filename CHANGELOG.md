# Changelog

All notable changes to AgentGuardVS-CXX will be documented in this file.

## v0.1.0 - Initial Public Release

### Added

- Agent-friendly CLI commands:
  - `analyze`
  - `verify`
  - `review`
  - `run`
  - `llm-smoke`
- Isolated workspace creation with an independent Git baseline.
- Diff isolation that rejects parent Git repositories.
- Semantic scope model with `allowed`, `context`, `suspected`, `needs_approval`, and `protected` files.
- LLM-assisted semantic analyze and semantic review.
- Provider adapters for fake, file, OpenAI-compatible, DeepSeek-compatible, and Claude-compatible workflows.
- MSBuild verification and build-log parsing.
- Markdown and JSON audit reports.
- Repository-level Codex Skill and installation scripts.
- Reproducible `examples/library-system` demo.
- Lightweight symbol and dependency impact analysis.
- Windows CI workflow, library demo workflow, and prototype `agentguard-review` action.

### Current Limits

- Focused on Visual Studio C++ and MSBuild.
- LLM semantic judgment may be incomplete or incorrect.
- Build success does not prove business correctness.
- Enterprise-scale repositories still require human review and policy tuning.
- Current validation is centered on small Visual Studio C++ examples and controlled tests.
