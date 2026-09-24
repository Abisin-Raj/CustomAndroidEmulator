@echo off
title Custom Android Emulator GUI - Redmi Note 7 Pro
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
cd /d "%~dp0build\launcher"
echo ================================================================================
echo Starting Custom Android Emulator GUI (Launcher)
echo ================================================================================
echo.
start "" "launcher.exe" %*

