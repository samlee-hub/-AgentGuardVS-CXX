@echo off
setlocal
cd /d "%~dp0.."
if "%AGENTGUARD_BENCH_PYTHON%"=="" set "AGENTGUARD_BENCH_PYTHON=python"
"%AGENTGUARD_BENCH_PYTHON%" tools\generate_report.py %*
exit /b %ERRORLEVEL%
