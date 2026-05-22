@echo off
setlocal
cd /d "%~dp0.."
if "%AGENTGUARD_BENCH_PYTHON%"=="" set "AGENTGUARD_BENCH_PYTHON=python"
"%AGENTGUARD_BENCH_PYTHON%" tools\prepare_run.py --task T001_state_unit_count --mode codex_raw
"%AGENTGUARD_BENCH_PYTHON%" tools\prepare_run.py --task T001_state_unit_count --mode codex_guarded
exit /b %ERRORLEVEL%
