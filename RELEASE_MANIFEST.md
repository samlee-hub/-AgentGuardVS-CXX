# AgentGuardVS-CXX Release Manifest

- **Project**: AgentGuardVS-CXX
- **Version**: 0.2.0
- **Generated**: 2026-06-06 02:48:49 +08:00
- **Build configuration**: Release
- **Binary included**: True
- **Binary source**: build\release-check\Release\AgentGuardVS.exe
- **Release tree size**: 2.10 MB

## Included top-level directories

- .github
- action
- benchmarks
- bin
- docs
- examples
- plugins
- scripts
- skills
- src
- tests

## Included top-level files

- README.md
- LICENSE
- CHANGELOG.md
- CONTRIBUTING.md
- SECURITY.md
- .env.example
- .gitignore
- CMakeLists.txt
- CMakePresets.json
- vcpkg.json

## Excluded from the release

- Version control / IDE: `.git/`, `.vs/`
- Build output: `build/`, `out/`, `dist/`, `x64/`, `Win32/`, `Debug/`, `Release/`, `obj/`, `vcpkg_installed/`
- Runtime / history: `runs/` (including `benchmarks/AgentGuardBench/runs/`), `TestResults/`
- Large benchmark run snapshots and isolated mirror repositories
- Internal planning drafts: `docs/superpowers/`
- Build intermediates: `*.obj`, `*.pdb`, `*.ilk`, `*.lib`, `*.exp`, `*.tlog`, `*.idb`, `*.iobj`, `*.binlog`, `*.log`, `*.tmp`, `*.bak`, archives

## Test results

100% tests passed, 0 failed out of 117 (Release, ctest 6.49s)

## How to build

```powershell
cmake -S . -B build/vs2022-release -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build/vs2022-release --config Release
ctest --test-dir build/vs2022-release -C Release --output-on-failure
```

## How to run

```powershell
# If you staged the local binary it is at bin/AgentGuardVS.exe; otherwise use your build output.
.\bin\AgentGuardVS.exe detect --project . --json
.\bin\AgentGuardVS.exe verify-profile --project . --json
.\bin\AgentGuardVS.exe analyze --project <path-to-project> --json
```
