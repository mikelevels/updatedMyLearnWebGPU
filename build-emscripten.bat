@echo off
REM ---------------------------------------------------------------------------
REM Build the browser (Emscripten) version of updatedMyLearnWebGPU.
REM
REM WHY THIS SCRIPT EXISTS:
REM Your default emsdk (C:\Users\gonza\Dev\Lib\emsdk, Emscripten 3.1.63) stays
REM on PATH because MyLearnWebGPU needs it. This project needs the LATEST
REM Emscripten, which lives in a SEPARATE emsdk folder. `setlocal` + calling
REM emsdk_env.bat switches to it ONLY inside this script: as soon as the
REM script ends, your terminal is back on 3.1.63.
REM
REM Usage (from any terminal, in this folder):
REM     build-emscripten.bat          configure (first time) and build
REM     build-emscripten.bat run      same, then serve and open in the browser
REM ---------------------------------------------------------------------------
setlocal

set EMSDK_LATEST=C:\Users\gonza\Dev\Lib\emsdk-latest
set EMSDK_QUIET=1
call "%EMSDK_LATEST%\emsdk_env.bat"
if errorlevel 1 (
    echo Could not activate %EMSDK_LATEST%
    exit /b 1
)
call emcc --version | findstr /b "emcc"

REM Configure only once: emcmake wraps cmake so that it uses Emscripten's
REM compiler instead of Visual Studio's.
if not exist build-emscripten\build.ninja (
    call emcmake cmake -B build-emscripten -G Ninja
    if errorlevel 1 exit /b 1
)

cmake --build build-emscripten
if errorlevel 1 exit /b 1

if /i "%1"=="run" (
    REM emrun starts a small local web server and opens the page.
    REM Stop it with Ctrl+C.
    call emrun build-emscripten\App.html
)

endlocal
