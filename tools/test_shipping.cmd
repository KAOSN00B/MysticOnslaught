@echo off
rem Thin wrapper kept for existing habits: runs every C++ suite via tools\run_tests.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_tests.ps1" %*
exit /b %errorlevel%
