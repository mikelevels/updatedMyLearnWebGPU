
Step000 (updatedMyLearnWebGPU)

This is the SAME app as MyLearnWebGPU, on the NEW WebGPU API and the LATEST Emscripten (6.0.12).
It follows the "Next" section of the guide (second table of contents on the guide's home page):
https://eliemichel.github.io/LearnWebGPU/
The differences with MyLearnWebGPU are written down chapter by chapter in MyNotes/migratingToTheNextGuide.md.

IMPORTANT:
FIRST - The desktop builds (WGPU, DAWN) work from any terminal: CMake and Ninja are on your user PATH now
(the old "x64 command prompt" step is not needed anymore).
SECOND - The browser build MUST go through build-emscripten.bat. It switches to the LATEST emsdk
(C:\Users\gonza\Dev\Lib\emsdk-latest) only while it runs. Your default emsdk stays 3.1.63 for MyLearnWebGPU.
DO NOT run "emsdk activate --permanent" in emsdk-latest: it would break MyLearnWebGPU's browser build.

# Build using wgpu-native backend
cmake -B build-wgpu -DWEBGPU_BACKEND=WGPU
cmake --build build-wgpu
.\build-wgpu\Debug\App.exe

# Build using Dawn backend
cmake -B build-dawn -DWEBGPU_BACKEND=DAWN
cmake --build build-dawn
.\build-dawn\Debug\App.exe

# Build using emscripten (then open the page, Ctrl+C then Y to stop the server)
.\build-emscripten.bat run

Step039_cpp

Port of MyLearnWebGPU Step039_cpp (first uniform) plus the fixes of MyLearnWebGPU commit b2c9b11
(instance kept alive, surface texture released every frame, no more exit(1)).

Versions: Emscripten 6.0.12 + emdawnwebgpu v20250517.163311, Dawn chromium/7187 (prebuilt dll),
wgpu-native v24.0.3.1, WebGPU-distribution main @ 17dcd42, glfw3webgpu v1.3.0-alpha, GLFW 3.4.0.

IMPORTANT: -sUSE_WEBGPU was REMOVED from Emscripten in 4.0.18. The browser build uses the emdawnwebgpu
port instead (WEBGPU_BACKEND=EMDAWNWEBGPU, picked automatically under emcmake). Never set
WEBGPU_BACKEND=EMSCRIPTEN here.

IMPORTANT: The wrapper's wgpu::Default marks EVERY part of a BindGroupLayoutEntry as used. wgpu-native refused
the pipeline ("does not match the pipeline binding (Texture)"). Mark sampler/texture/storageTexture as
BindingNotUsed.

NOTE: Dawn prints "Error: DXC dlls were built, but are not available" at start. Harmless: it falls back from
D3D12 to Vulkan (backend type 6).

NOTE: Dawn prints "Device lost: reason 3 (...)" when the window closes. Expected.

CONFIRMED All three builds run (wgpu, Dawn, emscripten 6.0.12). Memory stays flat on both desktop builds.

Step043_cpp

More uniforms: same code as MyLearnWebGPU Step043 (color + time + gamma, 32 bytes, static_asserts).
No API difference. Limits stay at their defaults, so nothing to raise. See MyNotes/migratingToTheNextGuide.md.

CONFIRMED All three builds run (wgpu, Dawn, emscripten 6.0.12).

Step044_cpp

Dynamic uniforms: two logos, two uniform blocks uniformStride (256) bytes apart, dynamic offsets in
setBindGroup(). device.getLimits() now fills a wgpu::Limits and returns a Status. No limit had to be raised
(MyLearnWebGPU had to raise maxDynamicUniformBuffersPerPipelineLayout and compute maxBufferSize).

CONFIRMED All three builds run (wgpu, Dawn, emscripten 6.0.12).

Step050_cpp

A simple example: same code as MyLearnWebGPU Step050 (pyramid.txt, loadGeometry `dimensions`, Float32x3,
stride 6 floats, rotation in the shader, blank-line loader fix, dynamic uniforms rolled back).
No limit to raise. NOTE: faces overlap in the WRONG order, expected (no depth buffer yet).

CONFIRMED All three builds run (wgpu, Dawn, emscripten 6.0.12).

HERE
