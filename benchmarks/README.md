# Benchmarks

These benchmarks are not a model leaderboard. They are small, reproducible checks for AgentGuardVS-CXX itself:

- Does semantic scope analysis produce a controlled write surface?
- Does the workflow keep edits inside an isolated workspace?
- Does MSBuild verification run and produce artifacts?
- Does semantic review produce a machine-readable decision?
- Do reports capture scope, build, review, and risk information?

The default benchmark path uses the `fake` provider and does not require network access or API keys. Real LLM providers can be used for smoke testing semantic quality, but those runs are environment-dependent and should not be compared as authoritative model scores.

Run the offline benchmark set from the repository root:

```powershell
.\scripts\run-benchmarks.ps1 -Provider fake
```

Optional live-provider run:

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "deepseek"
$env:AGENTGUARD_DEEPSEEK_MODEL = "<model>"
$env:DEEPSEEK_API_KEY = "<deepseek-api-key>"
.\scripts\run-benchmarks.ps1 -Provider deepseek -Model $env:AGENTGUARD_DEEPSEEK_MODEL
```

Benchmark run artifacts are written under `runs\benchmarks\...` and must not be committed.
