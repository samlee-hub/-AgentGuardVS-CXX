# AgentGuardVS-CXX local environment sample.
# Copy commands from this file into your shell profile or a private setup script.
# Never commit real API keys.

$env:AGENTGUARD_EXE = "<path-to-AgentGuardVS.exe>"
$env:AGENTGUARD_LLM_PROVIDER = "fake" # fake, file, openai, deepseek, or claude

$env:AGENTGUARD_OPENAI_MODEL = "<openai-model>"
$env:OPENAI_API_KEY = "<openai-api-key>"

$env:AGENTGUARD_DEEPSEEK_MODEL = "<deepseek-model>"
$env:DEEPSEEK_API_KEY = "<deepseek-api-key>"

$env:AGENTGUARD_CLAUDE_MODEL = "<claude-model>"
$env:ANTHROPIC_API_KEY = "<anthropic-api-key>"
