# Release Package Layout

A local release package should let a Windows user run AgentGuardVS-CXX without understanding the source tree.

Recommended archive layout:

```text
release/
  AgentGuardVS.exe
  skills/
    agentguard-vs-cxx/
      SKILL.md
      scripts/
      references/
  plugins/
  scripts/
    install.ps1
    uninstall.ps1
    doctor.ps1
    install-plugin.ps1
    setup-env.example.ps1
  docs/
  examples/
  README.md
  LICENSE
```

Minimum manual acceptance:

1. Extract the package into a clean directory.
2. Run `.\scripts\install.ps1`.
3. Open a new PowerShell window.
4. Run `agentguard --version`.
5. Run `.\scripts\doctor.ps1`.
6. Run `.\examples\run-library-demo.ps1 -Provider fake`.
7. Confirm the demo finishes without a real API key and reports that edits happened only inside an isolated workspace.

Release packages must not include:

- Real API keys or local environment files.
- `build\`, `.vs\`, `runs\`, `x64\`, `Win32\`, logs, or raw provider response artifacts from local testing.
- User-specific paths such as home directories or drive-letter project locations.
