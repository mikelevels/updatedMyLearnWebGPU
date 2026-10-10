# Every bug we caught, explained

This is a study sheet. Each entry shows:
- **Symptom**: what you actually saw (or would have seen);
- **Bad**: the code that caused it;
- **Good**: the fixed code;
- **Why**: what the rule is, so you can spot it yourself next time.

Most of these were caught in MyLearnWebGPU and carried over to updatedMyLearnWebGPU already fixed. The last section is the 9-bug review of updatedMyLearnWebGPU.

The bugs fall into a few families. Learn the family and you will spot the next one:

| Family | Entries |
|---|---|
| **Lifetime**: something released too early, too late, or never | 1, 2, 3, 4, 5 |
| **Reading past the end** of memory or of a file | 6, 7, 8 |
| **Silent garbage**: errors that are not reported | 9, 10, 11 |
| **Backend differences**: code that is right on one backend only | 12, 13, 14, 15, 16 |
| **Math**: two ways of building "the same" matrix that are not the same | 17, 18 |
| **Build system**: the build does not do what you think | 19, 20, 21, 22, 23, 24 |
| **Window and surface**: the surface must be configured again | 26 |
| **Wrong comments**: the code is fine, the explanation lies | 25 |

---

## Lifetime

### 1. The surface texture leaked every frame

**Symptom:** Dawn's memory grew about 70 KB per second while the window was open. Nothing visible at first; after hours it would run out of memory.

**Bad** (MyLearnWebGPU up to Step055):
```cpp
wgpu::SurfaceTexture surfaceTexture;
surface.getCurrentTexture(&surfaceTexture);
wgpu::Texture texture = surfaceTexture.texture;
return texture.createView(viewDescriptor);   // the texture is never released
```

**Good** (ApplicationFrame.cpp):
```cpp
currentSurfaceTexture = surfaceTexture.texture;   // keep it for the whole frame
return currentSurfaceTexture.createView(viewDescriptor);
// ... later, in PresentFrame():
surface.present();
currentSurfaceTexture.release();                  // AFTER present()
currentSurfaceTexture = nullptr;
```

**Why:** `getCurrentTexture()` hands you a NEW reference every frame. Anything WebGPU hands you, you release. The wrapper objects (`wgpu::Texture`, `wgpu::Buffer`...) are NOT smart pointers: going out of scope releases nothing. The order matters too: wgpu-native throws the frame away if the texture is released before `present()`.

### 2. The instance was released too early

**Symptom:** worked by luck on wgpu-native; on Dawn the instance's event system shuts down, so async callbacks (device lost, errors) can stop arriving.

**Bad:**
```cpp
wgpu::Instance instance = wgpuCreateInstance(nullptr);
wgpu::Adapter adapter = instance.requestAdapter(opts);
instance.release();   // "we have the adapter, we are done with it"
```

**Good** (Application.h + Application.cpp):
```cpp
wgpu::Instance instance = nullptr;   // a member
// ... used by processEvents() every frame ...
// Terminate(): released LAST, after everything created from it
instance.release();
```

**Why:** release objects in the reverse order you created them. The instance is the parent of everything.

### 3. Terminate() was never called

**Symptom:** nothing visible, because the OS frees everything when the program exits. But GPU validation layers report leaks, and it becomes a real bug the day the app opens a second window or restarts the renderer.

**Bad:**
```cpp
while (app.IsRunning()) { app.MainLoop(); }
return 0;
```

**Good** (main.cpp):
```cpp
while (app.IsRunning()) { app.MainLoop(); }
app.Terminate();
return 0;
```

### 4. Per-frame logging

**Symptom:** the console scrolled non-stop and the app ran slower (printing is slow, and in the browser every line goes through JavaScript).

**Bad:**
```cpp
std::cout << "Submitting command..." << std::endl;   // in the main loop
queue.submit(1, &command);
```

**Good:** print once during setup only. ApplicationFrame.cpp `SubmitCommands()` carries a comment saying why there is no print.

**Why:** `std::endl` also flushes the stream every time, which is the slow part.

### 5. A StringView pointing at a dead temporary

**Symptom:** none seen yet. That is the danger: it reads freed memory, so it can print garbage in an error message one day, on one machine.

**Bad** (the guide):
```cpp
shaderDesc.label = wgpu::StringView(path.string());   // path.string() dies at the ';'
return device.createShaderModule(shaderDesc);          // reads the dead string
```

**Good** (ResourceManager.cpp):
```cpp
std::string label = path.string();   // a named variable lives until the '}'
shaderDesc.label = wgpu::StringView(label);
return device.createShaderModule(shaderDesc);
```

**Why:** a `StringView`, like every pointer inside a WebGPU descriptor, only POINTS to data. The data must still be alive when the API call reads the descriptor. This is also THE rule the refactor had to respect (see the milestone notes): when a descriptor is built in a helper function, whatever it points to must outlive the helper.

---

## Reading past the end

### 6. The index buffer upload read 2 bytes past the vector

**Symptom:** usually nothing; sometimes 2 random bytes uploaded to the GPU. An address sanitizer would flag it.

**Bad:**
```cpp
// writeBuffer sizes must be a multiple of 4. 3 indices x 2 bytes = 6, so:
bufferDesc.size = (indexData.size() * sizeof(uint16_t) + 3) & ~3;   // 8
queue.writeBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size); // reads 8 of 6 bytes!
```

**Good** (ApplicationResources.cpp):
```cpp
indexCount = static_cast<uint32_t>(indexData.size());   // saved BEFORE padding
// Pad the DATA instead, so the vector really owns every byte we upload
if (indexData.size() % 2 != 0) {
    indexData.push_back(0);   // a dummy index, never drawn (drawIndexed uses indexCount)
}
bufferDesc.size = indexData.size() * sizeof(uint16_t);   // now a multiple of 4
queue.writeBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);
```

**Why:** rounding the SIZE up does not make the memory bigger. Round the data up.

### 7. The parser read one extra line at the end of the file

**Symptom:** a fake last vertex or triangle made of whatever was left in the variables.

**Bad:**
```cpp
while (!file.eof()) {
    getline(file, line);   // the last call fails, but we still process `line`
    ...
}
```

**Good** (ResourceManager.cpp):
```cpp
while (getline(file, line)) {   // false as soon as nothing was read
    ...
}
```

**Why:** `eof()` only becomes true AFTER a read has failed, so the loop runs once too often.

### 8. Reading the shader with a size measured by tellg()

**Symptom:** on Windows, possible garbage characters at the end of the shader text, and a WGSL compile error pointing past the last line. (Found during the refactor.)

**Bad:**
```cpp
file.seekg(0, std::ios::end);
size_t size = file.tellg();          // bytes ON DISK, "\r\n" = 2 bytes
std::string text(size, ' ');
file.seekg(0);
file.read(text.data(), size);        // text mode turns "\r\n" into "\n": fewer chars arrive
```

**Good** (ResourceManager.cpp `readTextFile()`):
```cpp
text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
```

**Why:** in text mode the number of characters you read is not the number of bytes on disk. Read until the end instead of trusting a size measured beforehand.

---

## Silent garbage

### 9. The parser accepted malformed lines

**Symptom:** a typo in a geometry file gave a strange mesh, with no error message.

**Bad:**
```cpp
float value;
for (int i = 0; i < 6; ++i) {
    iss >> value;              // fails on "abc", value keeps its OLD content
    pointData.push_back(value);
}
```

**Good** (ResourceManager.cpp `readNumbers()`):
```cpp
for (int i = 0; i < count; ++i) {
    if (!(iss >> value)) {
        return false;          // the whole file is reported as invalid
    }
    output.push_back(value);
}
```

**Why:** always check whether a read worked. A stream that failed does not tell you unless you ask.

### 10. A line with one space was not blank

**Symptom:** the guide's `pyramid.txt` ends with a line holding a single space. The guide's parser added a fake triangle (4, 4, 4); our stricter parser (entry 9) refused the whole file and nothing was drawn.

**Bad:**
```cpp
if (line.empty()) continue;
```

**Good** (ResourceManager.cpp):
```cpp
bool isBlankOrComment(const std::string& line) {
    size_t firstChar = line.find_first_not_of(" \t");
    return firstChar == std::string::npos || line[firstChar] == '#';
}
```

**Why:** the two bugs hid each other. Fixing entry 9 is what revealed entry 10. That is normal: a stricter check surfaces bad input that was always there.

### 11. No null checks on window, adapter, device

**Symptom:** on a machine without a usable GPU, a crash (access violation) with no message, instead of "Could not get a WebGPU adapter!".

**Bad:**
```cpp
wgpu::Adapter adapter = requestAdapterSync(instance, &opts);
adapter.getInfo(&info);   // crashes if adapter is null
```

**Good** (ApplicationDevice.cpp):
```cpp
wgpu::Adapter adapter = requestAdapterSync(instance, &adapterOpts);
if (!adapter) {
    std::cerr << "Could not get a WebGPU adapter!" << std::endl;
    return nullptr;
}
```

**Why:** every request can fail. Check, print what failed, and return false so `main()` can stop cleanly.

---

## Backend differences

### 12. Chrome rejected maxInterStageShaderComponents (Step037)

**Symptom:** the browser build stopped at "Requesting device..." with a device error; desktop worked.

**Bad:**
```cpp
requiredLimits.limits.maxInterStageShaderComponents = 3;   // Chrome removed this limit
```

**Good** (MyLearnWebGPU):
```cpp
#ifndef __EMSCRIPTEN__
requiredLimits.limits.maxInterStageShaderComponents = 3;
#endif
```
updatedMyLearnWebGPU does not set limits at all (entry 16), and the limit no longer exists in the new API.

**Why:** browsers move faster than native libraries. A limit can disappear.

### 13. The gamma result was computed and then not used

**Symptom:** the colors were darker or washed out depending on the backend: wgpu-native picks an sRGB surface, Dawn and the browser do not.

**Bad:**
```cpp
float gamma = isSrgb ? 2.2f : 1.0f;   // computed...
// ...but the shader kept its own hard-coded value
```

**Good** (ApplicationResources.cpp `ChooseGamma()`):
```cpp
gamma = isSrgb ? 2.2f : 1.0f;
uniforms.gamma = gamma;   // the shader reads it from the uniforms
```
and the clear color uses the same gamma: `std::pow(0.05, gamma)`.

**Why:** a compiler warning would not catch this (the variable is "used" in the assignment). Ask: where does this value END UP?

### 14. BindingNotUsed (wgpu-native rejected the pipeline)

**Symptom:** wgpu-native: a validation error when creating the bind group layout; Dawn and the browser worked.

**Bad:**
```cpp
wgpu::BindGroupLayoutEntry bindingLayout = wgpu::Default;
bindingLayout.buffer.type = wgpu::BufferBindingType::Uniform;
// sampler/texture/storageTexture left as wgpu::Default gave them
```

**Good** (ApplicationPipeline.cpp):
```cpp
bindingLayout.sampler.type = wgpu::SamplerBindingType::BindingNotUsed;
bindingLayout.texture.sampleType = wgpu::TextureSampleType::BindingNotUsed;
bindingLayout.storageTexture.access = wgpu::StorageTextureAccess::BindingNotUsed;
```

**Why:** in the new API, "Undefined" means "used, with default settings". The wgpu-native wrapper's Default fills Undefined, so it looked like one entry was a buffer AND a sampler AND a texture. Say what you mean explicitly instead of relying on a default that differs per backend.

### 15. depthClearValue and depthSlice left at the wrapper's default

**Symptom:** nothing drawn at all (depth cleared to 0.0 = "nearest", every fragment fails the `Less` test), or a validation error on Dawn/browser (NaN with `LoadOp::Clear`).

**Bad:**
```cpp
wgpu::RenderPassDepthStencilAttachment depthAttachment = wgpu::Default;
depthAttachment.view = depthTextureView;
```

**Good** (ApplicationFrame.cpp `MakeDepthAttachment()`):
```cpp
depthAttachment.depthClearValue = 1.0f;   // "as far as possible"
```
Same idea for `colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;`.

### 16. Limits set too tight

**Symptom** (MyLearnWebGPU, several times): "uniform binding too large", "buffer too big", "stride too large" errors on wgpu-native ONLY, every time the tutorial grew. Dawn rounds small requests up, wgpu-native enforces them exactly.

**Bad:**
```cpp
requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4;   // then the struct grew to 224
```

**Good** (updatedMyLearnWebGPU): do not request limits at all; the defaults (65536 bytes here) are enough. Ask for more only when you really need it, and then ask for the adapter's supported value.

---

## Math

### 17. The hand-written projection was not glm::perspective

**Symptom:** switching between the hand-built matrices and the GLM ones made the pyramid jump 25% in size. The guide has the same mismatch.

**Bad** (shader.wgsl, from the guide):
```wgsl
focalLength,         0.0,              0.0, 0.0,
    0.0,     focalLength * ratio,      0.0, 0.0,   // y stretched by 640/480
```

**Good** (shader.wgsl and Transforms.cpp `makePerspectiveByHand()`):
```wgsl
focalLength / ratio, 0.0,              0.0, 0.0,   // x squeezed by the ratio
    0.0,     focalLength,              0.0, 0.0,   // y untouched, like glm
```
Both keep the right aspect ratio, but the guide's is 4/3 bigger overall.

**Why:** when you have two ways to build the same thing, TEST that they agree (compare the numbers once), otherwise one silently drifts.

### 18. R1 spun the wrong way

**Symptom:** after Step055 the pyramid spun the opposite way compared with Step054.

**Bad:**
```cpp
mat4x4 R1 = glm::rotate(mat4x4(1.0), angle1, vec3(0.0, 0.0, 1.0));
```

**Good** (ApplicationFrame.cpp):
```cpp
mat4x4 R1 = glm::rotate(mat4x4(1.0), -angle1, vec3(0.0, 0.0, 1.0));
```

**Why:** the hand-written matrices are written row by row and then transposed, and the transpose of a rotation turns the other way. The `-angle2` of R2 was already right; R1 was missed.

---

## Build system

### 19. The browser kept the old shader

**Symptom:** you edit `shader.wgsl`, rebuild, reload: nothing changed. Ninja says "no work to do".

**Fix** (CMakeLists.txt): `LINK_DEPENDS` on every file in `resources/`, so a change re-links `App.html` and re-packs `App.data`.

### 20. An old webgpu_dawn.dll next to App.exe

**Symptom:** after upgrading Dawn, the app still behaved like the old Dawn.

**Fix:** `LINK_DEPENDS` on `$<TARGET_FILE:webgpu_dawn>`: the DLL is copied when App is re-linked, so we make the DLL a reason to re-link.

### 21. Dawn 8093: "_NULLABLE" in the generated header, a link clash, and "Windows Error: 87"

**Symptoms:** a compile error in webgpu.hpp; `LNK2005 StringView::operator string_view already defined`; "Couldn't load Vulkan" and no GPU found.

**Fixes:** `regenerate-dawn-hpp.bat` patches the generator output; Dawn as a DLL (`SHARED`); `DAWN_FORCE_SYSTEM_COMPONENT_LOAD=ON`. Details in migratingToTheNextGuide.md.

### 22. Browser resources only worked with DEV_MODE=ON (review bug 2)

**Symptom:** with `DEV_MODE=OFF` the browser could not find `shader.wgsl`; with ON, your `C:/Users/...` path was baked into the public web page.

**Bad:**
```cmake
--preload-file "${CMAKE_CURRENT_SOURCE_DIR}/resources"   # mounted at the full Windows path
```

**Good:**
```cmake
--preload-file "${CMAKE_CURRENT_SOURCE_DIR}/resources@/resources"
# and RESOURCE_DIR="/resources" for EMSCRIPTEN in both modes
```

### 23. cmake_minimum_required too low (review bug 3)

**Symptom:** none with your CMake. With CMake 3.16-3.24, `SYSTEM` and `COMPILE_WARNING_AS_ERROR` are silently ignored: the warnings-as-errors you rely on would vanish without a message.

**Good:** `cmake_minimum_required(VERSION 3.25...3.31)`.

### 24. .gitignore matched nothing

**Symptom:** build folders and `.vs/` showed up in `git status` and `.vs/` got committed.

**Bad:** `./build*/` (a leading `./` never matches). **Good:** `/build*/`.

---

## Window and surface

### 26. The window froze after minimize/restore (review bug 1)

**Symptom:** on wgpu-native/Vulkan, after minimizing and restoring, the window could stay frozen on the last picture.

**Bad:**
```cpp
if (surfaceTexture.status != SuccessOptimal && ... != SuccessSuboptimal) {
    return nullptr;   // skip this frame... and every frame after it
}
```

**Good** (ApplicationFrame.cpp `RecoverSurface()`):
```cpp
if (status == Outdated || status == Lost) {
    glfwGetFramebufferSize(window, &width, &height);
    if (width > 0 && height > 0) {   // 0 x 0 while minimized: try next frame
        surface.configure(surfaceConfig);
    }
}
```

**Why:** "Outdated" means "configure me again". Skipping only treats the symptom for one frame.

---

## Wrong comments

### 25. Four comments that lied (review bugs 6-9)

The code was fine, but a learner trusts comments. Fixed:
- "return false instead of exit(1) so we clean up": no, `main()` returns without `Terminate()` on that path.
- "we use 16 bytes" of uniforms: it is 224.
- "the only limits we may forward": any supported value can be forwarded; those two are special because LOWER is better.
- what `wgpu::Default` fills in: true on one backend only (entries 14, 15).

**Why:** when code changes, its comment must change in the same edit. A wrong comment is worse than none.

---

## How to catch these yourself

1. **Run all three backends every time.** Entries 12-16 only showed up on ONE of them.
2. **Watch memory for a minute** (Task Manager) on a new render loop. That is how entry 1 was found.
3. **Minimize and restore** the window. Entry 26.
4. **Feed the parser a broken file** on purpose. Entries 9-10.
5. **Compare two implementations of the same math** with printed numbers. Entries 17-18.
6. **Edit only a resource and rebuild.** Entry 19.
7. **Read every comment you did not write today** as if it might be wrong. Entry 25.
