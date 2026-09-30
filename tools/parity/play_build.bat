@echo off
REM Build a copy of the Magic Portals port to PLAY, in this repository's build-play\, from any shell.
REM It is port_build.bat's build - MSVC, Ninja, Release, the same Build Tools and glslc - with Vulkan
REM validation OFF: the layers check every Vulkan call, work a player's build does not do. build\ stays
REM the validated build that the suites and the parity captures run on; this one builds only the game, no
REM test_mp_* suite.
REM Its data paths are build\'s: the first run copies the SUPERSONIC_MAGICPORTALS_* entries from
REM build\CMakeCache.txt (the defaults when build\ has no cache yet). It configures ONCE: if build\'s paths
REM move, delete build-play\ and run this again.
REM Usage: tools\parity\play_build.bat [cmake --build args...]   (default: --target MagicPortals)
REM Then:  build-play\game\MagicPortals.exe
setlocal
for %%I in ("%~dp0..\..") do set "REPO=%%~fI"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%REPO%\engine\CMakeLists.txt" (
    echo engine\ is empty: the Supersonic Engine is a submodule. Run  git submodule update --init  first.
    exit /b 1
)
if exist "%REPO%\build-play\CMakeCache.txt" goto build
set "DATAPATHS="
if not exist "%REPO%\build\CMakeCache.txt" goto configure
for /f "usebackq tokens=1,* delims==" %%A in (`findstr /b /r /c:"SUPERSONIC_MAGICPORTALS_[A-Z]*:[A-Z]*=" "%REPO%\build\CMakeCache.txt"`) do call :datapath "%%A" "%%B"
:configure
echo play_build: the data paths copied from build\CMakeCache.txt (none when build\ has no cache: the project's defaults):%DATAPATHS%
cmake -S "%REPO%" -B "%REPO%\build-play" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=OFF -DMAGICPORTALS_BUILD_TESTS=OFF -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGLSL_COMPILER=C:/VulkanSDK/1.4.357.0/Bin/glslc.exe %DATAPATHS% || exit /b 1
:build
if "%~1"=="" (
    cmake --build "%REPO%\build-play" --target MagicPortals
) else (
    cmake --build "%REPO%\build-play" %*
)
exit /b %ERRORLEVEL%

:datapath
set DATAPATHS=%DATAPATHS% "-D%~1=%~2"
exit /b 0
