@echo off
REM ---------------------------------------------------------------------------
REM Build the desktop app with Dawn chromium/8093, built FROM SOURCE.
REM
REM WHY THIS SCRIPT EXISTS:
REM Dawn is compiled together with the app, with Ninja and the Visual Studio
REM compiler. vcvars64.bat sets up the compiler for this script only.
REM The FIRST build downloads Dawn and its dependencies (several GB) and takes
REM a long time. Later builds only recompile what changed.
REM
REM Usage (from any terminal):
REM     build-dawn.bat          configure (first time) and build
REM     build-dawn.bat run      same, then start the app
REM ---------------------------------------------------------------------------
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo Could not set up the Visual Studio x64 compiler
    exit /b 1
)

REM Always work from the folder this script is in
cd /d "%~dp0"

if not exist build-dawn\build.ninja (
    REM Python 3.12 is pinned because Python 3.8 and 3.9 are also installed
    cmake -G Ninja -B build-dawn -DCMAKE_BUILD_TYPE=Debug -DWEBGPU_BACKEND=DAWN ^
        -DPython3_EXECUTABLE=C:/Users/gonza/AppData/Local/Programs/Python/Python312/python.exe
    if errorlevel 1 exit /b 1
)

cmake --build build-dawn --target App
if errorlevel 1 exit /b 1

if /i "%1"=="run" (
    build-dawn\App.exe
)

endlocal
