# AgentGuard Generalization Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the first generic project profile and command verification layer without breaking existing Visual Studio C++ behavior.

**Architecture:** Introduce `src/project` as a focused module for project detection, config loading, and configured command execution. Existing VS-specific commands continue to use the current `.sln` and MSBuild path, while new `detect` and `verify-profile` commands expose the generalized layer.

**Tech Stack:** C++20, nlohmann/json, CLI11, existing `ProcessRunner`, GoogleTest, CMake.

---

### Task 1: Project Profile Model And Detection

**Files:**
- Create: `src/project/ProjectProfile.h`
- Create: `src/project/ProjectProfile.cpp`
- Test: `tests/test_project_profile.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Create tests for Visual Studio C++, Node, and explicit `agentguard.json` detection.

- [ ] **Step 2: Run tests and confirm failure**

Run: `cmake --build build\codex-vs2022-debug --config Debug --target test_project_profile`

Expected: build fails because `project/ProjectProfile.h` does not exist.

- [ ] **Step 3: Implement model and detection**

Add `ProjectProfile`, `CommandStep`, `DetectProjectProfile`, and JSON conversion helpers.

- [ ] **Step 4: Run profile tests**

Run: `ctest --test-dir build\codex-vs2022-debug -C Debug -R ProjectProfile --output-on-failure`

Expected: all `ProjectProfileTest` tests pass.

### Task 2: Command Profile Verification

**Files:**
- Create: `src/project/CommandVerifier.h`
- Create: `src/project/CommandVerifier.cpp`
- Test: `tests/test_command_verifier.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Create tests for a passing command and a failing command using `cmd.exe`.

- [ ] **Step 2: Run tests and confirm failure**

Run: `cmake --build build\codex-vs2022-debug --config Debug --target test_command_verifier`

Expected: build fails because `project/CommandVerifier.h` does not exist.

- [ ] **Step 3: Implement command verifier**

Use the existing `IProcessRunner` interface and record per-step return code, stdout, stderr, and duration.

- [ ] **Step 4: Run verifier tests**

Run: `ctest --test-dir build\codex-vs2022-debug -C Debug -R CommandVerifier --output-on-failure`

Expected: all `CommandVerifierTest` tests pass.

### Task 3: CLI Surface

**Files:**
- Modify: `src/main.cpp`
- Modify: `tests/test_cli_agent_commands.cpp`

- [ ] **Step 1: Write failing CLI tests**

Add tests for `detect --project <root> --json` and `verify-profile --project <root> --json`.

- [ ] **Step 2: Run tests and confirm failure**

Run: `ctest --test-dir build\codex-vs2022-debug -C Debug -R CliAgentCommands --output-on-failure`

Expected: CLI tests fail because the new commands do not exist.

- [ ] **Step 3: Implement CLI commands**

Add `detect` and `verify-profile` while leaving existing commands untouched.

- [ ] **Step 4: Run CLI tests**

Run: `ctest --test-dir build\codex-vs2022-debug -C Debug -R CliAgentCommands --output-on-failure`

Expected: CLI tests pass.

### Task 4: Full Regression

**Files:**
- No new files.

- [ ] **Step 1: Build**

Run: `cmake --build build\codex-vs2022-debug --config Debug`

Expected: build succeeds.

- [ ] **Step 2: Full test suite**

Run: `ctest --test-dir build\codex-vs2022-debug -C Debug --output-on-failure`

Expected: all tests pass.

- [ ] **Step 3: Verify source boundaries**

Run: `git status --short`

Expected: only files under `D:\AgentGuardVS-CXX` are modified.
