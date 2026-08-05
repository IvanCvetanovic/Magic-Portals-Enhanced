@echo off
REM Forwarder so the Godot path lives in exactly one place. See CLAUDE.md -> Environment.
REM Installed 2026-08-04 via: winget install GodotEngine.GodotEngine --version 4.6.3
"%LOCALAPPDATA%\Microsoft\WinGet\Packages\GodotEngine.GodotEngine_Microsoft.Winget.Source_8wekyb3d8bbwe\Godot_v4.6.3-stable_win64.exe" %*
