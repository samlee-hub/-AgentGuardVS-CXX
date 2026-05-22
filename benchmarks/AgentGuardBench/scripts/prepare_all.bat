@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0.."
if "%AGENTGUARD_BENCH_PYTHON%"=="" set "AGENTGUARD_BENCH_PYTHON=python"
for /d %%T in (tasks\T*) do (
  "%AGENTGUARD_BENCH_PYTHON%" tools\prepare_run.py --task %%~nxT --mode codex_raw
  if errorlevel 1 exit /b !ERRORLEVEL!
  "%AGENTGUARD_BENCH_PYTHON%" tools\prepare_run.py --task %%~nxT --mode codex_guarded
  if errorlevel 1 exit /b !ERRORLEVEL!
)
exit /b 0
