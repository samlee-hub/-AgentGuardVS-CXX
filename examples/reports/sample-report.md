# AgentGuardVS-CXX Sample Report

This is a sanitized example of the report shape produced by the library-system demo.

## User Task

Add case-insensitive title keyword search to `LibrarySystem` and add console self-tests.

## Workspace Audit

- Source project: `examples/library-system`
- Workspace repo: `<runs-root>/<task-id>/repo`
- Git top-level: `<runs-root>/<task-id>/repo`
- Diff base: `agentguard baseline`
- Source modified: `false`
- Workspace modified: `true`

## Related Files

- `LibraryApp/LibrarySystem.h`
- `LibraryApp/LibrarySystem.cpp`
- `LibraryApp/LibraryTests.cpp`

## Build Result

- Success: `true`
- Return code: `0`

## Semantic Review

- next_action: `accept`
- risks: none in the sample run

## Impact Analysis

- Related symbols: `LibrarySystem`, `FindBook`
- Dependency reasons: `LibrarySystem.h` is included by implementation and self-test files.
- Blast radius: `medium`
- Public interface changes: `true`

## Notes

The original project is not edited. The sample patch is applied only to the isolated workspace created by AgentGuardVS-CXX.
