@echo off
REM ---------------------------------------------------------------------------
REM Regenerate webgpu/dawn/include/webgpu/webgpu.hpp (the C++ wrapper) from the
REM webgpu.h of the Dawn version we build. Needed ONLY when DAWN_VERSION changes.
REM
REM Needs: a configured build-dawn folder (build-dawn.bat) and the WebGPU-Cpp
REM generator in C:\Users\gonza\Dev\Lib\WebGPU-Cpp (commit b9507d9).
REM ---------------------------------------------------------------------------
setlocal
cd /d "%~dp0"

set GENERATOR=C:\Users\gonza\Dev\Lib\WebGPU-Cpp\generate.py
set HEADER=build-dawn\_deps\dawn-build\gen\include\dawn\webgpu.h
set OUTPUT=webgpu\dawn\include\webgpu\webgpu.hpp

if not exist "%HEADER%" (
    echo %HEADER% not found: run build-dawn.bat first
    exit /b 1
)

py -3.12 "%GENERATOR%" -u "%HEADER%" -o "%OUTPUT%" --use-init-macros
if errorlevel 1 exit /b 1

REM GENERATOR BUG: Dawn's wgpuDeviceCreateBuffer now returns "WGPU_NULLABLE
REM WGPUBuffer", and the generator writes "_NULLABLE WGPUBuffer createBuffer(",
REM which is not valid C++. Fix it to "Buffer createBuffer(".
py -3.12 -c "p=r'%OUTPUT%'; s=open(p,encoding='utf-8').read(); s=s.replace('_NULLABLE WGPUBuffer createBuffer(','Buffer createBuffer(').replace('_NULLABLE WGPUBuffer Device::createBuffer(','Buffer Device::createBuffer('); open(p,'w',encoding='utf-8').write(s); assert '_NULLABLE' not in s"
if errorlevel 1 exit /b 1

echo Regenerated %OUTPUT%
endlocal
