@echo off
setlocal
cd /d "%~dp0.."
if "%AGENTGUARD_BENCH_PYTHON%"=="" set "AGENTGUARD_BENCH_PYTHON=python"
"%AGENTGUARD_BENCH_PYTHON%" tools\verify_all.py %*
exit /b %ERRORLEVEL%
