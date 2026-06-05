# Changelog

All notable changes to AgentGuardVS-CXX will be documented in this file.

## v0.2.0 - 2026-06-06 - Multi-Project ProjectProfile Upgrade

### Added

- General `ProjectProfile` layer with `ProjectKind`, `LanguageKind`, `SourceFilePolicy`, stable profile JSON, confidence, reasons, and `profile_source`.
- Project detection for `visualstudio-cxx`, `cmake-cpp`, `python`, `node`, `dotnet`, `java`, `unreal`, `custom`, and `unknown`.
- Multi-language source indexing for C/C++, Python, JavaScript/TypeScript, Java, .NET, Unreal metadata, and common project config files.
- `agentguard.json` overrides for project kind, languages, include/exclude rules, source extensions, max file size, and verification commands.
- `verify_commands` with command, args, working directory, timeout, required/optional status, and missing-executable skip behavior.
- `verify --project <root> --json` and `doctor --project <root> --json`.
- Audit report fields for project kind, languages, profile source, indexed/skipped file counts, verify commands, verify result, modified files, and forbidden write attempts.

### Changed

- `analyze --project <root>` now creates an isolated workspace, detects a profile, indexes files through `SourceFilePolicy`, and writes profile audit metadata.
- `verify-profile` now uses the same `CommandVerifier` model as `verify --project`.
- The semantic analysis prompt now describes an AgentGuard multi-project task instead of a Visual Studio C++-only task.
- Documentation and Skill metadata now describe `AgentGuardVS.exe` as a legacy binary name, not a language boundary.

### Compatibility

- Existing Visual Studio C++ `.sln` / `.vcxproj` analyze, run, verify-workspace, review, and MSBuild tests remain supported.
- The end-to-end `run` repair loop remains the mature Visual Studio C++ path; non-VS projects are supported through profile-based analyze, isolated workspace scope, command verification, review, and audit artifacts.

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
