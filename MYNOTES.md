
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

# Build using Dawn backend (Dawn 8093 from source, see build-dawn.bat)
.\build-dawn.bat run

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

Step052_cpp

Depth buffer: same as MyLearnWebGPU Step052. NEW API: depthWriteEnabled is an OptionalBool
(wgpu::OptionalBool::True). Texture size limits not needed (defaults). depthClearValue MUST still be set to 1.0.

CONFIRMED All three builds run (wgpu, Dawn, emscripten 6.0.12). Faces in the RIGHT order.

Step054_cpp

Transformation matrices: shader only, identical to MyLearnWebGPU Step054 (WGSL did not change).

CONFIRMED All three builds run (wgpu, Dawn, emscripten 6.0.12).

Step055_cpp

Projection matrices: identical to MyLearnWebGPU Step055 (GLM 0.9.9.8 copied, 3 matrices in the uniforms,
perspective). No limit to raise (MyLearnWebGPU had to raise maxUniformBufferBindingSize to 256).

CONFIRMED All three builds run (wgpu, Dawn, emscripten 6.0.12). SAME CAPABILITIES AS MyLearnWebGPU NOW.

Dawn8093_and_bug_fixes

Dawn is now chromium/8093 (the latest), BUILT FROM SOURCE. Use build-dawn.bat (the first build downloads GBs
and takes a long time). webgpu/dawn/include/webgpu/webgpu.hpp was regenerated (regenerate-dawn-hpp.bat).
Fixed on the way: a generator bug (_NULLABLE), a link clash (Dawn is now a DLL), "Couldn't load Vulkan
Windows Error 87" (DAWN_FORCE_SYSTEM_COMPONENT_LOAD=ON) and an OLD dll left next to App.exe.

IMPORTANT: The old "cmake -B build-dawn -DWEBGPU_BACKEND=DAWN" line from Step000 is replaced by build-dawn.bat.

BUG REVIEW: 9 bugs confirmed and fixed (window could freeze after minimize/restore, browser resources with
DEV_MODE=OFF, CMake minimum version, hand projection != glm::perspective, spin direction flipped, 4 false
comments). Table in MyNotes/migratingToTheNextGuide.md.

CONFIRMED All three builds run (wgpu, Dawn 8093, emscripten 6.0.12).

Milestone1_refactor

MILESTONE: read MyNotes/milestone1.md (everything so far: goals, both projects, versions, decisions,
how the code is organized now, what comes next) and MyNotes/bugsExplained.md (all 26 bugs caught so far,
bad vs good code, symptoms, how to catch them yourself).

REFACTOR: no function is longer than 25 lines (the longest was 110). main.cpp only holds main() now.
The Application class is split by topic: Application.cpp (big steps), ApplicationDevice.cpp,
ApplicationPipeline.cpp, ApplicationResources.cpp, ApplicationFrame.cpp. The hand-written matrices are in
Transforms.cpp, and WEBGPU_CPP_IMPLEMENTATION has its own file (webgpu-implementation.cpp).
The map of which file holds what is at the top of Application.h.

FIXED on the way: the shader file was read with a size measured by tellg(), which does not match what a
Windows text-mode read returns (CRLF). It now reads until the end of the file.

CONFIRMED All three builds run (wgpu, Dawn 8093, emscripten 6.0.12), minimize/restore still draws.

HERE
