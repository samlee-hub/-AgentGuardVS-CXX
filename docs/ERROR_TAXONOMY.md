# Error Taxonomy

## Purpose

This document defines the initial build-error categories used by AgentGuardVS-CXX when interpreting Visual Studio C++ build output.

## Primary Categories

### Include Errors

Typical examples:

- `C1083`: Cannot open include file or source file.

Likely causes:

- Missing or misspelled header path.
- Incorrect relative include path.
- Required include directory missing from project configuration.

### Compile Errors

Typical examples:

- `C2065`: Undeclared identifier.
- `C2143`: Syntax error, often caused by a missing token such as `;`, `)`, or `}`.
- `C2664`: No matching function overload or invalid argument conversion.
- `C3861`: Identifier not found, commonly an unknown function name.

Likely causes:

- Declaration and usage drift.
- Incorrect types or function signatures.
- Missing namespace qualification.
- Incomplete refactor across headers and implementation files.

### Link Errors

Typical examples:

- `LNK2019`: Unresolved external symbol.
- `LNK1104`: Cannot open file during link.

Likely causes:

- Declaration exists but definition is missing.
- New `.cpp` file is not compiled into the project.
- Library or output file path is wrong.
- Build artifact is locked or missing.

### MSBuild Errors

Typical examples:

- Any code beginning with `MSB`, such as `MSB3073`.

Likely causes:

- Custom build step failure.
- Invalid command in project configuration.
- Missing toolchain path or unsupported build event.

### Configuration Errors

Configuration problems may be represented by MSBuild output or platform-related build failures:

- Missing platform toolset.
- Invalid configuration or platform name.
- Inconsistent solution versus project build mappings.

### Unknown

Errors that do not match the initial taxonomy remain in an explicit unknown bucket and must still be preserved in raw form for reporting.

## Initial Code-to-Category Mapping

| Code | Category |
| --- | --- |
| `C1083` | Include Error |
| `C2065` | Compile Error |
| `C2143` | Compile Error |
| `C2664` | Compile Error |
| `C3861` | Compile Error |
| `LNK2019` | Link Error |
| `LNK1104` | Link Error |
| `MSB*` | MSBuild Error or Configuration Error |

## Design Rule

The taxonomy must remain concrete enough for automated classification, but broad enough to support both English and Chinese Visual Studio build output examples in later parser tests.
