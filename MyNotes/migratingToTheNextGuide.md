
Step_039_cpp (updatedMyLearnWebGPU)

# Migrating to the Next guide (old API vs new API)

## Why a second project

The main guide's chapters after "More uniforms" were written for an OLDER WebGPU. The guide's newer "Next" section (second table of contents on https://eliemichel.github.io/LearnWebGPU/) is written for the NEW `webgpu.h` that Dawn, wgpu-native and browsers now share. `MyLearnWebGPU` keeps following the main guide on its pinned 2024 versions. `updatedMyLearnWebGPU` is the SAME app on the new API, and every chapter's differences are written down here.

Once both projects can do the same things, we stop following the tutorial and continue only in this one.

## Versions

| | MyLearnWebGPU | updatedMyLearnWebGPU |
|---|---|---|
| Emscripten | 3.1.63 (`C:\Users\gonza\Dev\Lib\emsdk`, on PATH) | **6.0.12**, the latest (`C:\Users\gonza\Dev\Lib\emsdk-latest`, NOT on PATH) |
| WebGPU in the browser | built into Emscripten (`-sUSE_WEBGPU`) | **emdawnwebgpu** v20250517 (a port released by Dawn) |
| Dawn (desktop) | 6536 (2024) | 7187 (2025), prebuilt `webgpu_dawn.dll` |
| wgpu-native (desktop) | v0.19.4.1 | v24.0.3.1 |
| WebGPU-distribution (`webgpu/`) | 2024 commits | main @ 17dcd42 (v0.3.0-gamma +) |
| glfw3webgpu | old | v1.3.0-alpha |
| GLFW | 3.4.0 | 3.4.0 (same copy) |

>[!note]
>Dawn and wgpu-native are NOT the very latest. They are the versions the Next chapters are written and tested with, and `webgpu/` ships a `webgpu.hpp` generated for exactly them. The latest Dawn has no prebuilt that Visual Studio 2022 can link (it would have to be built from source, GBs and a long build) and needs a regenerated `webgpu.hpp` with hand fixes. That is a separate step for later.

## The build

### Emscripten: -sUSE_WEBGPU is gone

Emscripten deprecated its built-in WebGPU in 4.0.10 and REMOVED it in 4.0.18 (`emsdk-latest\upstream\emscripten\ChangeLog.md`). WebGPU in the browser now comes from **emdawnwebgpu**, which is maintained by the Dawn team. With `emcmake`, WebGPU-distribution picks `WEBGPU_BACKEND=EMDAWNWEBGPU` by itself: it downloads the port and adds `--use-port=.../emdawnwebgpu.port.py` to the compile AND link options.

>[!IMPORTANT]
>Never set `WEBGPU_BACKEND=EMSCRIPTEN` here: that one still adds `-sUSE_WEBGPU`, which Emscripten 6 refuses. `CMakeLists.txt` stops with a clear message if you do.

### Two emsdk folders

emsdk can only hold ONE installed Emscripten per folder, so the latest lives in a second folder: `C:\Users\gonza\Dev\Lib\emsdk-latest`. It is NOT on PATH (your default stays 3.1.63 for MyLearnWebGPU). `build-emscripten.bat` switches to it only while it runs:

```
build-emscripten.bat          configure (first time) and build
build-emscripten.bat run      same, then serve with emrun and open the browser
```

>[!IMPORTANT]
>Emscripten 6 needs Python 3.10 or newer. Your user environment has `EMSDK_PYTHON` pointing at the old emsdk's Python 3.9, so ANY new-Emscripten command typed by hand in a normal terminal fails. Always go through `build-emscripten.bat`, or run `C:\Users\gonza\Dev\Lib\emsdk-latest\emsdk_env.bat` first in that terminal.

### glfw3webgpu

v1.3.0-alpha renamed the function: `glfwGetWGPUSurface()` is now `glfwCreateWindowWGPUSurface()`. In the browser it targets the page's `canvas`.

### Dawn prints an error that is not one

```
Error: DXC dlls were built, but are not available
    at Initialize (...\d3d12\BackendD3D12.cpp:130)
```

The prebuilt Dawn 7187 tries its D3D12 backend first and does not ship the DXC compiler DLLs it needs. It then uses **Vulkan** instead ("backend type 6"), and everything works. Harmless.

## The code

Every change is marked `[NEW API]` in `main.cpp`, `ResourceManager.cpp` and `webgpu-utils.cpp`.

### Strings are StringView

Labels and messages are a `WGPUStringView` = pointer + LENGTH. A plain `"..."` no longer compiles as a label:

```Cpp
deviceDesc.label = wgpu::StringView("My Device");
```

>[!IMPORTANT]
>A StringView only POINTS to characters, it does not own them. `StringView(path.string())` points into a temporary string that is destroyed at the end of the line. Keep the `std::string` in a named variable first.

Strings that WebGPU gives US (error messages, adapter name) may NOT end with `'\0'`. Never print `message.data` directly: use `toStdStringView(message)` (webgpu-utils.cpp).

### Requests are asynchronous

`instance.requestAdapter(opts)` and `adapter.requestDevice(desc)` used to return the object. Now every request takes a callback INFO: the callback, two userdata pointers and a **mode**, and the answer comes later. `webgpu-utils.cpp` has `requestAdapterSync()` and `requestDeviceSync()` (from the Next guide) that wait for the answer by calling `wgpuInstanceProcessEvents()` in a loop. In the browser the wait uses `emscripten_sleep()`, which needs `-sASYNCIFY`.

>[!IMPORTANT]
>The mode has NO valid default value: 0 is refused by Dawn and the browser. We set `WGPUCallbackMode_AllowProcessEvents`. And the wgpu-native v24 header has no `..._INIT` macros, so the structs are zero-initialized with `= {}` and every field is set by hand.

### The instance stays alive

The instance is now a class member, released LAST. `MainLoop()` calls `instance.processEvents()` every frame, which replaces the old `#if` block (`device.tick()` for Dawn, `device.poll(false)` for wgpu-native). One line, all three builds.

### Callbacks go in the device descriptor

`deviceLostCallback` became `deviceLostCallbackInfo` (callback + mode + userdata), and `device.setUncapturedErrorCallback()` is gone: the error callback is given in the descriptor BEFORE the device exists (`uncapturedErrorCallbackInfo`). No more `std::unique_ptr` handle to keep alive.

### Limits

`RequiredLimits` / `SupportedLimits` are gone: both are a plain `wgpu::Limits` (no `.limits.` in the middle). `maxInterStageShaderComponents` does not exist anymore, so the `#ifndef __EMSCRIPTEN__` around it disappears.

>[!note]
>The Next guide no longer asks for the SMALLEST limits the app fits in. It leaves everything at Default and only RAISES a limit when a chapter needs more. That avoids the trap we hit in MyLearnWebGPU, where wgpu-native enforces small limits exactly while Dawn silently rounds them up.

### Surface format

`surface.getPreferredFormat(adapter)` is gone. We ask for the surface **capabilities**; the preferred format is the first one. Copy it before `capabilities.freeMembers()`.

### Getting the surface texture

- There is no plain `Success` status anymore: `SuccessOptimal` (normal) and `SuccessSuboptimal` (works, but would like a reconfigure, e.g. after a resize).
- The texture is kept until the END of the frame and released after `surface.present()` on ALL three builds. No `#ifdef` (MyLearnWebGPU needed one: release early on Dawn/browser, after present on wgpu-native).

### depthSlice exists everywhere

`renderPassColorAttachment.depthSlice` exists on all backends now, so the `#ifndef WEBGPU_BACKEND_WGPU` around it is gone.

### Shader module

`ShaderModuleWGSLDescriptor` is now `ShaderSourceWGSL`, its `code` is a StringView, and the wgpu-native-only `hints` fields (and their `#ifdef`) are gone.

### Bind group layout: BindingNotUsed (found by testing!)

A `BindGroupLayoutEntry` has 4 parts (buffer, sampler, texture, storageTexture) and only ONE may be used. In the new API:

- `BindingNotUsed` = "this part is not used"
- `Undefined` = "used, with the default settings"

The wrapper's `wgpu::Default` sets all four parts to `Undefined`, i.e. USED. On the first run, wgpu-native saw a TEXTURE at `@binding(0)` and refused the pipeline: "Type on the shader side (Buffer) does not match the pipeline binding (Texture)". Fix:

```Cpp
wgpu::BindGroupLayoutEntry bindingLayout = wgpu::Default;
bindingLayout.sampler.type = wgpu::SamplerBindingType::BindingNotUsed;
bindingLayout.texture.sampleType = wgpu::TextureSampleType::BindingNotUsed;
bindingLayout.storageTexture.access = wgpu::StorageTextureAccess::BindingNotUsed;
```

### Releasing the device destroys it

With Dawn, closing the window prints `Device lost: reason 3 (A valid external Instance reference no longer exists.)`. Expected.

## What did NOT change

The WGSL shader is byte for byte the same. `loadGeometry`, the vertex layout, the uniform struct, gamma and the bind group are the same ideas with new spellings.

## Conclusion

Most differences are SPELLING (StringView, Limits, ShaderSourceWGSL, capabilities). The real behavior changes are: requests are asynchronous (callback + mode + processEvents), callbacks are given up front in descriptors, and "not used" vs "default" is now explicit. Three `#ifdef`s disappeared because all backends agree now.

CONFIRMED WGPU (wgpu-native v24.0.3.1), DAWN (Dawn 7187) and EMSCRIPTEN 6.0.12 (emdawnwebgpu v20250517) all run the moving WebGPU logo.

---

# Chapter by chapter

## Step043: More uniforms

**Code: identical to MyLearnWebGPU.** The uniform struct (`color` first, then `time`, `gamma`, padding to 32 bytes), the two `static_assert`s and the shader tint are copied as they are. Uniforms, `writeBuffer` and `offsetof` did not change in the new API.

**One thing we did NOT have to touch:** `maxUniformBufferBindingSize`. In MyLearnWebGPU it was set to 64 bytes (16 floats) and had to be raised later, at Step055, because wgpu-native enforces limits exactly. Here the limits stay at their defaults (65536 bytes), so the struct can grow without touching the limits.

CONFIRMED Step043 runs on WGPU, DAWN and EMSCRIPTEN 6.0.12 (logo tinted green on all three).

## Step044: Dynamic uniforms

**Same idea, same result:** two uniform blocks in one buffer, `uniformStride` (256 bytes here too) apart, and each draw picks its block with a dynamic offset in `setBindGroup()`.

| | MyLearnWebGPU | updatedMyLearnWebGPU |
|---|---|---|
| Reading the device limits | `wgpu::SupportedLimits l; device.getLimits(&l);` then `l.limits.minUniformBufferOffsetAlignment` | `wgpu::Limits l = wgpu::Default;` and `device.getLimits(&l)` returns a `wgpu::Status` that we CHECK, then `l.minUniformBufferOffsetAlignment` |
| `maxDynamicUniformBuffersPerPipelineLayout` | had to be raised to 1 (we asked for the minimum, 0) | nothing to do: the default is 8 |
| `maxBufferSize` | had to be COMPUTED (the logo's 300 bytes only fit 288 by luck) | nothing to do: the default is 256 MB |

>[!note]
>This is the payoff of the Next guide's "leave limits at Default" rule: two of the three changes this chapter needed in MyLearnWebGPU simply do not exist here.

CONFIRMED Step044 runs on WGPU, DAWN and EMSCRIPTEN 6.0.12 (two logos, the second one 70% see-through, stride 256 bytes).

## Step050: A simple example

**Code: identical to MyLearnWebGPU,** including our two deviations from the guide:
- `loadGeometry` treats lines with only spaces as blank (the guide's `pyramid.txt` ends with one);
- the dynamic uniforms of Step044 are rolled back, like in the guide.

The vertex layout changes (`Float32x3` position, color at offset 3 floats, stride 6 floats) and the rotation in the shader do not touch the API at all.

| | MyLearnWebGPU | updatedMyLearnWebGPU |
|---|---|---|
| `maxVertexBufferArrayStride` | had to be raised from 5 to 6 floats | nothing to do (default 2048 bytes) |

CONFIRMED Step050 runs on WGPU, DAWN and EMSCRIPTEN 6.0.12 (rotating pyramid, faces still in the wrong order: no depth buffer yet).

## Step052: Depth buffer

Same depth texture (`Depth24Plus`, 640 x 480), same view, same depth test (`Less`), same depth attachment (cleared to 1.0, stencil ops `Undefined` on all three, no `#ifdef`). The spelling changes:

| | MyLearnWebGPU | updatedMyLearnWebGPU |
|---|---|---|
| Write the depth | `depthWriteEnabled = true;` | `depthWriteEnabled = wgpu::OptionalBool::True;` (it is an OptionalBool now: True / False / Undefined, `= true` does not compile) |
| Descriptors | `wgpu::TextureDescriptor d;` | `wgpu::TextureDescriptor d = wgpu::Default;` plus a `label` (StringView) so errors name the texture |
| Texture size limits | had to be set (`maxTextureDimension2D = 640` ...) | nothing to do (default 8192) |

>[!IMPORTANT]
>`wgpu::Default` does NOT set `depthClearValue` on the depth attachment: it would stay 0.0, which means "nearest", and every fragment would fail the `Less` test (nothing drawn). We set it to 1.0 explicitly, like before.

CONFIRMED Step052 runs on WGPU, DAWN and EMSCRIPTEN 6.0.12 (faces now overlap in the right order).

## Step054: Transformation matrices

**Shader only, and WGSL did not change between the two APIs:** the shader is byte for byte the one of MyLearnWebGPU Step054 (S, T, R1, R2, applied as `R2 * R1 * T * S`, rows wrapped in `transpose()`).

The "browser keeps the old shader" trap does not exist here: `CMakeLists.txt` had the resource `LINK_DEPENDS` from the start (MyLearnWebGPU only got it at Step054). Checked: editing only `shader.wgsl` re-links `App.html`.

CONFIRMED Step054 runs on WGPU, DAWN and EMSCRIPTEN 6.0.12 (small spinning pyramid, seen from the side).

## Step055: Projection matrices

**Code: identical to MyLearnWebGPU Step055** (projection, view and model matrices built on the CPU with GLM, 224-byte uniform struct with the matrices first, model matrix re-uploaded every frame, same shader). GLM is the same 0.9.9.8 copy, header-only, and `CMakeLists.txt` gets the same two lines (project root on the include path, MSVC warning C4201 off). GLM compiles without warnings with Emscripten 6.0.12's newer clang too.

| | MyLearnWebGPU | updatedMyLearnWebGPU |
|---|---|---|
| `maxUniformBufferBindingSize` | had to be raised from 64 to 256 bytes (wgpu-native would have refused the 224-byte struct) | nothing to do (default 65536 bytes) |

---

# Where the two projects stand now

Both projects show the same thing on all three builds: the green pyramid orbiting the center of the screen with real perspective and correct depth. **They now have the same capabilities.**

Over the six chapters, the NEW API only changed spellings (`getLimits` + `Status`, `OptionalBool`, `wgpu::Default` + labels). The biggest practical difference is the limits: MyLearnWebGPU had to raise or compute a limit in Step044, Step050, Step052 and Step055; updatedMyLearnWebGPU never had to, because it leaves them at their defaults.

CONFIRMED Step055 runs on WGPU, DAWN and EMSCRIPTEN 6.0.12 (perspective pyramid).

---

# Dawn 8093 (the latest) and the bug review

## Dawn chromium/8093, built from source

| | before | now |
|---|---|---|
| Dawn | 7187, prebuilt `webgpu_dawn.dll` | **8093** (latest, 2026-10-07), compiled with the app |
| How to build | `cmake -B build-dawn -DWEBGPU_BACKEND=DAWN` | `build-dawn.bat` (Ninja + Visual Studio compiler; the first build downloads GBs and takes a long time) |
| C++ wrapper | `webgpu.hpp` shipped with WebGPU-distribution | regenerated from Dawn 8093's `webgpu.h` (`regenerate-dawn-hpp.bat`) |

There is no prebuilt Dawn this recent that Visual Studio 2022 can link, so it is built from source. Four things broke on the way, all fixed:

1. **Generator bug.** The WebGPU-Cpp generator wrote `_NULLABLE WGPUBuffer createBuffer(...)`, which is not C++ (Dawn now marks that function `WGPU_NULLABLE`). `regenerate-dawn-hpp.bat` fixes it to `Buffer createBuffer(...)` after generating.
2. **Link clash.** Dawn's STATIC library contains Dawn's own copy of the C++ wrapper code, which clashed with ours: `StringView::operator string_view already defined`. Dawn is now built as a DLL (`DAWN_BUILD_MONOLITHIC_LIBRARY=SHARED`).
3. **"Couldn't load Vulkan ... Windows Error: 87".** Dawn asks Windows to load `vulkan-1.dll` with flags that only accept a FULL path, so the plain name fails with "invalid parameter" and no GPU is found. `DAWN_FORCE_SYSTEM_COMPONENT_LOAD=ON` makes Dawn load it from System32.
4. **Old DLL next to App.exe.** The DLL is copied next to `App.exe` only when `App.exe` is re-linked, so after rebuilding only Dawn the OLD DLL stayed there. `CMakeLists.txt` now re-links `App` whenever `webgpu_dawn.dll` changes.

>[!note]
>Dawn 8093 prints the surface format as **27** (BGRA8Unorm). Dawn 7187 printed 23. This is the renumbering warned about in the Step039 comments: compare formats by NAME.

>[!note]
>The prebuilt Dawn 7187 printed "DXC dlls were built, but are not available". Dawn 8093 is built with Vulkan only, so that message is gone.

## Bugs found by the review (and fixed)

A review hunted for real bugs; each one was checked by a second reviewer who tried to prove it wrong. 9 survived, 5 were rejected. Most of them are ALSO in MyLearnWebGPU (same code).

| # | Bug | Fix |
|---|---|---|
| 1 | After minimizing and restoring the window, the surface can report **Outdated/Lost**. We skipped the frame and never configured it again, so the window could stay frozen. | The surface settings are kept (`surfaceConfig`) and the surface is configured again on Outdated/Lost. Checked: minimize + restore keeps drawing. |
| 2 | **Browser files only found with DEV_MODE=ON**, and your Windows disk path was baked into the web page. | `--preload-file ...resources@/resources` and `RESOURCE_DIR="/resources"` for the web, in both modes. |
| 3 | `cmake_minimum_required(3.16)` but we use features from CMake 3.24/3.25 that an older CMake silently ignores. | Now 3.25. |
| 4 | The hand-written projection (Option A, and `makePerspectiveProj` in the shader) was NOT the same matrix as `glm::perspective`: the GLM picture was 25% smaller. The guide has the same mismatch. | The hand matrix now scales x by `focalLength/ratio` and y by `focalLength`, with w = z: exactly `glm::perspective`. |
| 5 | The hand-written R1 turns by MINUS the angle (it is transposed), `glm::rotate` by PLUS. The pyramid spun the other way than in Step054. | `glm::rotate(..., -angle1, ...)`, like the `-angle2` already used for R2. |
| 6 | The comment said returning false "instead of exit(1)" cleans up. It does not: `main()` returns 1 without `Terminate()`. | Comment corrected. |
| 7 | "we use 16 bytes" of uniform binding: it is 224. | Comment corrected. |
| 8 | "the only limits we may forward": any supported value may be forwarded; these two are special because LOWER is better. | Comment corrected. |
| 9 | Comments about what `wgpu::Default` fills in (depthSlice, depthClearValue, BindGroupLayoutEntry) were true on ONE backend only. | Comments now say what each wrapper does. |

CONFIRMED WGPU (wgpu-native v24), DAWN (Dawn 8093 from source) and EMSCRIPTEN 6.0.12 run.

