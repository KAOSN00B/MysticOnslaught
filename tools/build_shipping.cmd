@echo off
rem Thin wrapper kept for existing habits: builds Shipping via tools\build.ps1,
rem which discovers Visual Studio and raylib (MO_VS_INSTALL / MO_RAYLIB_DIR).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Configuration Shipping %*
exit /b %errorlevel%
