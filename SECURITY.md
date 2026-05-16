# Security Policy

AgentGuardVS-CXX is a local reliability and audit layer for Visual Studio C++ agent workflows. It reduces risk through isolated workspaces, scoped edits, build verification, semantic review, and report redaction, but it does not make LLM output inherently safe.

## Reporting Security Issues

Please do not open a public issue for a vulnerability that exposes secrets, bypasses protected files, escapes the workspace, or allows unsafe command execution. Use GitHub private vulnerability reporting when available, or contact the maintainers through a private channel listed on the repository profile.

Include:

- A concise description of the issue.
- Reproduction steps.
- Affected version or commit.
- Whether secrets, protected files, or workspace escape are involved.

## API Keys

Never commit real API keys. Keep provider credentials in environment variables or a secret manager:

- `OPENAI_API_KEY`
- `DEEPSEEK_API_KEY`
- `ANTHROPIC_API_KEY`

Do not place keys in repository files, Skill files, Plugin manifests, reports, logs, prompts, raw responses, cache files, or benchmark data.

## Log And Report Redaction

Reports redact common bearer tokens, API-key style values, and sensitive command output before writing `report.md` and `report.json`. Redaction is a defense-in-depth measure, not a reason to put secrets in prompts or build output.

If a raw provider response appears to contain a secret, LLM caching skips writing that response.

## Prompt Injection Risk

AgentGuardVS-CXX may include source snippets in LLM prompts for semantic analysis and review. Malicious comments or repository files can attempt prompt injection. The platform flags suspicious prompt-injection text and raises risk, but human review is still required.

## Current Security Boundaries

- Original target projects are not modified by the platform workflow.
- Edits happen in isolated `runs/<task_id>/repo` workspaces.
- `protected_files` are never valid edit targets.
- `needs_approval_files` require user confirmation before edits.
- Build success and LLM review do not prove business correctness.
- Enterprise projects still need policy tuning and human review.
