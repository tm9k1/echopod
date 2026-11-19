@echo off
setlocal
cd /d "%~dp0"
set INTERNAL_STORAGE=%~dp0internal_storage

build\sim\platform\sim\echo_sim.exe --scale 2 --internal "%INTERNAL_STORAGE%" --system-dir "%INTERNAL_STORAGE%\.echo"
if errorlevel 1 pause