# Milestone 1: where everything stands (2026-10-08)

Read this when you come back after a break. It covers both projects, what was built, the versions, the decisions you made, how the code is organized now, and what comes next.

Companion files:
- `bugsExplained.md`: every bug we caught, with bad vs good code and symptoms.
- `migratingToTheNextGuide.md`: per-chapter differences between the old and the new WebGPU API.
- `../MYNOTES.md`: your step-by-step log.

---

## 1. The goal

**Phase one:** a C++ graphics layer over WebGPU, learned by following the LearnWebGPU guide. It has to run three ways from one code base:

| Build flag | What it is | Where it runs |
|---|---|---|
| WGPU | wgpu-native (Rust implementation, from Firefox) | desktop |
| DAWN | Dawn (C++ implementation, from Chrome) | desktop |
| EMSCRIPTEN | your code compiled to WebAssembly, using the browser's WebGPU | browser |

**The end of phase one:** a 3D model (the mammoth) on screen, with GUI panels whose controls change graphics settings at runtime. The GUI goes through an interface the engine owns, with two implementations: Dear ImGui, and a homemade one we write together.

**Later:** the Vulkan engine in getIntoGameDev (local Vulkan interface, multithreaded version). Not touched yet, on purpose.

---

## 2. The two projects

| | MyLearnWebGPU | updatedMyLearnWebGPU |
|---|---|---|
| Follows | the main guide | the guide's "Next" chapters (new API) |
| WebGPU API | old webgpu.h (pinned versions) | new webgpu.h (StringView, callback infos, `wgpu::Limits`...) |
| Emscripten | 3.1.63 (`C:\Users\gonza\Dev\Lib\emsdk`, your default) | 6.0.12 (`C:\Users\gonza\Dev\Lib\emsdk-latest`, only through `build-emscripten.bat`) |
| Dawn | chromium/7187 prebuilt | **chromium/8093, built from source** |
| wgpu-native | (old pinned) | v24.0.3.1 |
| Repo | github.com/mikelevels/mylearnwgpu | github.com/mikelevels/updatedMyLearnWebGPU |
| Last commit | d51a526 Step055 | b8d0562 Step055 (+ this milestone, uncommitted) |
| Reached | Step055, perspective pyramid | Step055, same picture |

**They now have the same capabilities.** Your plan: once you are comfortable with the current code, only updatedMyLearnWebGPU moves forward, toward the engine.

### Chapters covered (both projects)

| Step | Chapter | What it added |
|---|---|---|
| 000-003 | Setup, adapter | CMake for 3 backends, getting a GPU |
| 005-015 | Device, command queue | the device, limits, first commands |
| 020-025 | Window, first color | GLFW window, surface, clear color |
| 028 | C++ wrapper | `webgpu.hpp` instead of the raw C API |
| 030-034 | Triangle, buffers | pipeline, vertex + index buffers |
| 037 | Loading from file | geometry and shader read from `resources/` |
| 039 | First uniform | a `time` value sent to the shader |
| 043 | More uniforms | uniform struct (color, time, gamma), alignment rules |
| 044 | Dynamic uniforms | two draws from one buffer (later rolled back) |
| 050 | A simple example | first 3D mesh, the pyramid |
| 052 | Depth buffer | faces overlap in the right order |
| 054 | Transformation matrices | scale, translate, rotate |
| 055 | Projection matrices | GLM, perspective, 224-byte uniform struct |

---

## 3. Decisions you made (keep these)

1. **Renderer-centric engine design**, not thin wrappers ("thin wrappers limit growth"). Condition: it must be OBVIOUS where WebGPU-specific code ends and Vulkan-specific code begins.
2. **Two tracks** until parity (done), then only the updated project.
3. **Leave WebGPU limits at their defaults** in the updated project (see bugsExplained.md entry 16).
4. **Dawn built from source** at the latest revision (8093).
5. **Commit workflow:** small commits; before each, all three builds are run and the code is explained; you give the command ("commit", "commit and push", "commit updated").
6. **Code style rule (new):** no function longer than 25 lines; no monolithic `main()`.

---

## 4. How to build and run

From `updatedMyLearnWebGPU`:

| Backend | Build | Run |
|---|---|---|
| WGPU | `cmake -B build-wgpu -DWEBGPU_BACKEND=WGPU` then `cmake --build build-wgpu` | `.\build-wgpu\Debug\App.exe` |
| DAWN | `.\build-dawn.bat` (first build: long, several GB) | `.\build-dawn.bat run` |
| EMSCRIPTEN | `.\build-emscripten.bat` | `.\build-emscripten.bat run` (serves the page and opens it; Ctrl+C then Y to stop) |

What you should see on all three: a green pyramid orbiting the center with perspective, faces in the right order, dark grey background.

| Backend | Surface format printed | sRGB |
|---|---|---|
| WGPU | 24 (BGRA8UnormSrgb) | yes, gamma 2.2 |
| DAWN 8093 | 27 (BGRA8Unorm) | no, gamma 1.0 |
| Browser | 23 (BGRA8Unorm in emdawnwebgpu's numbering) | no, gamma 1.0 |

The numbers differ because each header numbers the formats differently. Always compare formats by NAME in code.

---

## 5. The refactor (this milestone)

### Before and after

| | Before | After |
|---|---|---|
| Longest function | 110 code lines (212 with comments) | 25 code lines |
| `main.cpp` | 998 lines: the whole app | 36 lines: `main()` and the browser loop |
| Files | main.cpp + ResourceManager + webgpu-utils | 14 files, one topic each |

All three builds were run after the refactor: WGPU, DAWN and the browser draw the same picture with 0 errors, and both desktop builds keep drawing after a minimize/restore. A separate review compared every function with the old `main.cpp` and found no behavior change except the intended bug fixes.

### Where the code lives now

| File | What it does |
|---|---|
| `main.cpp` | creates the Application, runs the loop, calls Terminate |
| `Application.h` | the class, the uniform struct + its `static_assert`s, and a MAP of which file holds what |
| `Application.cpp` | the big steps: Initialize, Terminate, MainLoop, IsRunning |
| `ApplicationDevice.cpp` | window, instance, surface, adapter, device, surface configuration |
| `ApplicationPipeline.cpp` | shader, vertex layout, blend, bind group layout, render pipeline |
| `ApplicationResources.cpp` | geometry, uniform buffer, matrices (3 options), gamma, depth buffer, bind group |
| `ApplicationFrame.cpp` | one frame: surface texture, uniforms, render pass, submit, present, surface recovery |
| `Transforms.h/.cpp` | the hand-written matrices of Step054/055 |
| `webgpu-implementation.cpp` | the ONE file with `#define WEBGPU_CPP_IMPLEMENTATION` |
| `ResourceManager.cpp` | file loading (geometry, shaders) |
| `webgpu-utils.cpp` | requestAdapterSync / requestDeviceSync |

### The refactors, and why each is good practice

**1. Split by "level of abstraction".** `Initialize()` now reads like a table of contents:
```cpp
if (!InitializeWindow()) return false;
if (!InitializeDevice()) return false;
if (!InitializePipeline()) return false;
InitializeDepthBuffer();
if (!InitializeBuffers()) return false;
InitializeBindGroups();
```
*Why:* a function should either DECIDE the order of steps or DO one step, not both. You read the top level to understand the program, and open one step only when you need its details. This is the "Composed Method" pattern.

**2. One file per topic (Device / Pipeline / Resources / Frame).** One class, split across several `.cpp` files.
*Why:* when something goes wrong with the surface, you know which file to open. It is also the seam for your renderer-centric engine: the Device file is where a Vulkan version would differ most.

**3. "Make" functions that return a descriptor by value** (`MakeColorAttachment()`, `MakeDepthAttachment()`, `MakeDepthStencilState()`, `makeBlendState()`...).
*Why:* each one fills a WebGPU struct and gives it back, so it can be read, tested and reused on its own. They are `const` or free functions: they cannot change the app's state by accident.

**4. The objects a descriptor POINTS to stay in the caller.** Example in `InitializePipeline()`:
```cpp
std::vector<wgpu::VertexAttribute> vertexAttribs;
wgpu::VertexBufferLayout vertexBufferLayout = makeVertexBufferLayout(vertexAttribs);
wgpu::BlendState blendState = makeBlendState();
wgpu::ColorTargetState colorTarget = makeColorTarget(surfaceFormat, blendState);
// ... createRenderPipeline() reads all of them HERE, while they are alive
```
*Why:* this is the trap of splitting WebGPU code. If `makeColorTarget()` had its own local `BlendState` and returned a target pointing to it, the pointer would dangle as soon as the function returned (bugsExplained.md entry 5). Rule: **a helper may return a struct, but never a struct that points into the helper's own locals.**

**5. Helpers that are private to one file go in an anonymous namespace** (`onDeviceLost`, `printAdapterInfo`, `readNumbers`...).
*Why:* `namespace { }` makes them invisible outside their `.cpp`. The class header only lists what the class really is, and two files can have helpers with the same name without a link error.

**6. Named constants instead of magic numbers** (`kScale = 0.3f`, `kFocalLength = 2.0f`, `kNear = 0.01f`...).
*Why:* `0.3` tells you nothing; `kScale` does, and changing it in one place changes it everywhere. This is also where your future GUI sliders will plug in.

**7. Duplicate code removed.** The waiting loop of requestAdapterSync and requestDeviceSync is now one function, `waitUntilRequestEnded()`. The parser's two copies of "read N numbers" are one template, `readNumbers<T>()`.
*Why:* a bug fixed in one copy stays in the other ("Don't Repeat Yourself").

**8. The `WEBGPU_CPP_IMPLEMENTATION` define got its own file.**
*Why:* exactly one file may compile the wrapper's function bodies. When it hid at the top of the 998-line `main.cpp`, splitting `main.cpp` would have been easy to get wrong (link errors: "already defined" or "unresolved").

**9. The guide's comments were KEPT and moved with their code.**
*Why:* this is a learning project; the comments are the lesson. Comments that said "this is in MainLoop" were updated to the new function names.

### What was NOT refactored, on purpose

- **No RAII wrappers** (objects that release themselves). `release()` is still written by hand in `Terminate()`. You should first understand WHAT must be released and in which order; automating it is an engine step.
- **No abstraction over the backends.** The `#ifdef`s are still visible where the backends differ. That is the material for the renderer-centric design.
- **The three matrix "Options" (A shader, B by hand, C GLM) are all still there**, so you can compare them.

---

## 6. Bugs fixed so far (summary)

26 bugs over the two projects, in 7 families: lifetime, reading past the end, silent garbage, backend differences, math, build system, wrong comments. Full list with bad vs good code: **`bugsExplained.md`**.

The ones most worth remembering:
1. **Anything WebGPU hands you, you release**, in reverse order of creation (surface texture leak, instance).
2. **A descriptor only points; the data must be alive** when the API reads it (StringView label, the refactor rule).
3. **Run all three backends**: half the bugs only appeared on one of them.
4. **Check every read and every request**: a failed stream or a null adapter does not announce itself.

**Not yet fixed in MyLearnWebGPU:** the 9 bugs from the updatedMyLearnWebGPU review (most are in the old code too). Waiting for your OK.

---

## 7. Testing checklist (use before every commit)

1. Build all three. No warnings (they are errors here).
2. Run all three. Same picture? Same log, apart from the format number?
3. Minimize and restore (desktop). Still drawing?
4. Watch memory in Task Manager for a minute. Flat?
5. Edit only `shader.wgsl`, rebuild the browser build. Did the change show up?
6. Break `pyramid.txt` on purpose (a letter in a number). "Could not load geometry!"?

---

## 8. What comes next

1. **You:** read this file and `bugsExplained.md`, then say "commit updated" for this milestone.
2. **You:** decide whether the 9 review fixes go into MyLearnWebGPU too.
3. **Guide:** the next chapters (lighting, textures, loading a real model with tinyobjloader), toward the mammoth.
4. **Engine:** start the renderer-centric layer in updatedMyLearnWebGPU, with the WebGPU-specific code clearly separated (a folder or namespace per backend) so the Vulkan backend can sit next to it.
5. **GUI:** the engine-owned GUI interface, Dear ImGui first, then the homemade implementation.
6. **Window resizing:** the depth texture and the surface are still hard-coded to 640 x 480.
