# Project Brief

## Project Goal

AgentGuardVS-CXX is a C++20 platform for reliability verification and automated repair of AI coding agent changes applied to Visual Studio C++ projects.

Its purpose is to sit between an external AI agent and a real C++ engineering workspace:

1. Translate a user request into a structured task specification.
2. Restrict which files may or may not be modified.
3. Accept only structured patch plans from the agent.
4. Apply validated changes inside an isolated workspace.
5. Build, diagnose, and document the result.

## What the Project Is Not

AgentGuardVS-CXX is not:

- A replacement for Codex, Claude Code, or Cursor.
- A general-purpose chat assistant.
- A text-only diff generator with no engineering validation.
- A system that allows an LLM to write arbitrary files directly.
- A framework whose core execution path depends on Python scripts.

## Core Innovation

The project focuses on a reliability control layer for AI coding agents targeting Visual Studio C++ solutions:

- `TaskSpec` defines the request, target, acceptance expectations, and repair budget.
- `PatchPlan` provides a constrained machine-readable modification contract.
- `PatchApplier` is the only component allowed to touch files.
- `runs/<task_id>/repo` ensures all code edits occur in an isolated copy.
- Build verification, diagnostics, diff reporting, and run reports create an auditable engineering loop.

## Why This Is Not a Codex / Claude Code / Cursor Clone

Those tools primarily optimize agent interaction and code generation experience. AgentGuardVS-CXX optimizes the execution control layer around generated changes:

- It constrains the write surface before any file is changed.
- It validates behavior through engineering artifacts, not conversation alone.
- It treats build failures as structured repair inputs.
- It produces reusable reports, metrics, and benchmark-friendly artifacts.

The project is therefore closer to an AI reliability and software engineering infrastructure system than to an editor assistant.

## Why C++20 Is the Core Language

C++20 is selected intentionally:

- The target domain is real Visual Studio C++ engineering.
- Native Windows process control and filesystem handling are first-class concerns.
- The platform should integrate cleanly with CMake, MSBuild-oriented workflows, and Visual Studio debugging.
- The implementation should demonstrate systems-level engineering rather than delegating the reliability-critical core to scripting.

Python remains useful later for offline analytics, but not for the platform's core orchestration or safety logic.
