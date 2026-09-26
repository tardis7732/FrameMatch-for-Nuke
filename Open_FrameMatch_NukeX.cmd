@echo off
setlocal
set "NUKE_PATH=%~dp0nuke;%NUKE_PATH%"
set "OFX_PLUGIN_PATH=%~dp0ofx;%OFX_PLUGIN_PATH%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\launch_nuke.ps1" %*
if errorlevel 1 pause
