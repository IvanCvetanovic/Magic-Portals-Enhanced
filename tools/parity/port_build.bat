@echo off
REM Build the Supersonic Engine Magic Portals port from any shell, with MSVC's environment.
REM Machine-specific on purpose, like tools\godot.bat: the Build Tools path lives here once.
REM Usage: tools\parity\port_build.bat [cmake --build args...]   e.g.  --target MagicPortals
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake --build "%USERPROFILE%\Desktop\Supersonic-Engine\build" %*
