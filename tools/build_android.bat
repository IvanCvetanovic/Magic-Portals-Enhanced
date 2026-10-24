@echo off
REM Build and package Magic Portals Enhanced for Android (planning step 203). The work is tools/build_android.sh; this only runs it under
REM Git Bash: %MAGICPORTALS_GIT_BASH% when set, else Git for Windows' own at its default place. Options as there (--data or --no-data first;
REM build_android.bat --help lists them). The APK it makes was run on an x86_64 emulator in step 204 (docs/android.md section 0); NOT RUN ON ANY PHONE.
setlocal
set "GITBASH=%MAGICPORTALS_GIT_BASH%"
if not defined GITBASH set "GITBASH=%ProgramFiles%\Git\bin\bash.exe"
if exist "%GITBASH%" goto :run
echo No Git Bash at "%GITBASH%". Install Git for Windows, or set MAGICPORTALS_GIT_BASH to its bash.exe.
exit /b 1
:run
"%GITBASH%" "%~dp0build_android.sh" %*
