# updatedMyLearnWebGPU: the SAME app as MyLearnWebGPU Step039_cpp (commit 2bb72f4),
# ported to the new webgpu.h API and the latest Emscripten (checked 2026-10-08).
# It follows the "Next" table of contents of the guide:
#   https://eliemichel.github.io/LearnWebGPU/next/basic-3d-rendering/shader-uniforms/a-first-uniform.html
#   reference code: https://github.com/eliemichel/LearnWebGPU-Code/tree/step039-next
# 3.25 because we use the SYSTEM target property (3.25) and
# COMPILE_WARNING_AS_ERROR (3.24). An older CMake would silently ignore them.
cmake_minimum_required(VERSION 3.25...3.31)
project(
	LearnWebGPU
	VERSION 0.1.0
	LANGUAGES CXX C
)

#################################################
# Versions
# Emscripten is the LATEST: 6.0.12, in C:\Users\gonza\Dev\Lib\emsdk-latest
# (see build-emscripten.bat).
# Dawn is the LATEST too: chromium/8093, BUILT FROM SOURCE (there is no
# prebuilt Dawn this recent that Visual Studio 2022 can link). See
# build-dawn.bat. The webgpu.hpp in webgpu/dawn/include/webgpu/ was
# regenerated for it (see regenerate-dawn-hpp.bat).
# wgpu-native and emdawnwebgpu stay at the versions WebGPU-distribution (the
# webgpu/ folder, main @ 17dcd42) ships with, and so do their webgpu.hpp:
#   wgpu-native v24.0.3.1, emdawnwebgpu v20250517.163311.
# These are CACHE variables also declared inside webgpu/. Declaring them HERE
# first wins (set(... CACHE ...) never overwrites an existing entry). An
# existing build-* folder keeps the values it was made with: use a fresh one.
set(DAWN_VERSION "8093" CACHE STRING "Dawn revision, the N in chromium/N")
if (WEBGPU_BACKEND STREQUAL "DAWN")
	# Build Dawn from source (it is a LONG first build, several GB)
	set(WEBGPU_BUILD_FROM_SOURCE ON CACHE BOOL "Build the WebGPU implementation from source")
	# A DLL (webgpu_dawn.dll): the static library also contains Dawn's own C++
	# wrapper code, which clashes with our webgpu.hpp at link time
	# ("StringView::operator string_view already defined").
	set(DAWN_BUILD_MONOLITHIC_LIBRARY "SHARED" CACHE STRING "SHARED, STATIC or OFF")
	# Load vulkan-1.dll from System32. Without this, Dawn 8093 asks Windows to
	# load "vulkan-1.dll" with flags that only accept a FULL path, gets
	# "Windows Error: 87" (invalid parameter) and finds no GPU at all.
	set(DAWN_FORCE_SYSTEM_COMPONENT_LOAD ON CACHE BOOL "Load system components from System32")
	# Things we do not need, that only cost build time
	set(DAWN_USE_GLFW OFF CACHE BOOL "Dawn's GLFW windowing utils")
	set(DAWN_BUILD_PROTOBUF OFF CACHE BOOL "Build the protobuf dependencies")
	set(TINT_BUILD_TESTS OFF CACHE BOOL "Build Tint unit tests")
endif()

# Catch a mistake that otherwise gives a confusing error
string(TOUPPER "${WEBGPU_BACKEND}" APP_WEBGPU_BACKEND_U)
if (EMSCRIPTEN AND APP_WEBGPU_BACKEND_U STREQUAL "EMSCRIPTEN")
	# The EMSCRIPTEN backend adds -sUSE_WEBGPU, which emscripten removed in 4.0.18
	message(FATAL_ERROR "Do not set WEBGPU_BACKEND=EMSCRIPTEN, leave it empty (under emcmake the default is EMDAWNWEBGPU)")
endif()

#################################################
# Dependency subdirectories
if (NOT EMSCRIPTEN)
	add_subdirectory(glfw)
else()
	# Emscripten has built-in support for GLFW (-sUSE_GLFW=3 still exists in 6.0.12),
	# so we create a mock 'glfw' target that just sets the link option
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

# Defines the 'webgpu' target. Under emcmake it downloads emdawnwebgpu and adds
# --use-port=.../emdawnwebgpu.port.py to the compile AND link options for us.
add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)

# webgpu.hpp is generated code: mark the 'webgpu' include folders as SYSTEM so
# that /W4 + COMPILE_WARNING_AS_ERROR only judge OUR code (needs CMake 3.25+)
set_target_properties(webgpu PROPERTIES SYSTEM ON)

add_executable(App
	main.cpp
	# The Application class, split by topic (see the map in Application.h)
	Application.h
	Application.cpp
	ApplicationDevice.cpp
	ApplicationPipeline.cpp
	ApplicationResources.cpp
	ApplicationFrame.cpp
	# The hand-written matrices of Step054/Step055
	Transforms.h
	Transforms.cpp
	# The ONE file that compiles the webgpu.hpp wrapper's function bodies
	webgpu-implementation.cpp
	# Add the ResourceManager files
	ResourceManager.h
	ResourceManager.cpp
	# requestAdapterSync/requestDeviceSync/toStdStringView (Next guide, The Adapter)
	webgpu-utils.h
	webgpu-utils.cpp
)

target_link_libraries(App PRIVATE glfw webgpu glfw3webgpu)

# Step055: GLM is a header-only library copied into the glm/ folder at the
# root of the project (same copy as MyLearnWebGPU). Adding the project root
# to the include path is what makes `#include <glm/glm.hpp>` work.
target_include_directories(App PRIVATE .)

# We add an option to enable different settings when developing the app than
# when distributing it.
option(DEV_MODE "Set up development helper settings" ON)

if(EMSCRIPTEN)
    # In the browser, the resources folder is packed into App.data and mounted
    # at /resources in the virtual file system (see --preload-file below), in
    # BOTH modes. BUG FIX (bug review): before, the folder kept your absolute
    # Windows path inside the browser, so DEV_MODE=OFF ("./resources") could
    # not find the files, and your disk path was baked into the web page.
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="/resources"
    )
elseif(DEV_MODE)
    # In dev mode, we load resources from the source tree, so that when we
    # dynamically edit resources (like shaders), these are correctly
    # versionned.
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    )
else()
    # In release mode, we just load resources relatively to wherever the
    # executable is launched from, so that the binary is portable
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="./resources"
    )
endif()

# Copies wgpu_native.dll or webgpu_dawn.dll next to App.exe (does nothing for the web)
target_copy_webgpu_binaries(App)
# BUG FIX: that copy only happens when App.exe is re-linked. When only Dawn's
# DLL changed, App.exe was not re-linked and an OLD webgpu_dawn.dll stayed next
# to it. LINK_DEPENDS re-links App (and so re-copies) when the DLL changes.
if (TARGET webgpu_dawn)
	set_property(TARGET App APPEND PROPERTY LINK_DEPENDS $<TARGET_FILE:webgpu_dawn>)
endif()

set_target_properties(App PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	COMPILE_WARNING_AS_ERROR ON
)

# I think this is where the set all warnings as errors happens... Did this fix the issue?
# (Answer: /W4 and -Wall -Wextra turn the warnings ON, COMPILE_WARNING_AS_ERROR just
# above is what turns them into errors.)
if (MSVC)
	target_compile_options(App PRIVATE /W4)
	# Step055: GLM uses "nameless structs/unions", a common compiler extension
	# that MSVC warns about at /W4 (C4201). Since warnings are errors, we turn
	# this one off, like the guide does.
	target_compile_options(App PRIVATE /wd4201)
else()
	target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()

if (XCODE)
	set_target_properties(App PROPERTIES
		XCODE_GENERATE_SCHEME ON
		XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
	)
endif()

# Visual Studio: F5 runs App instead of ALL_BUILD (from the Next guide)
# NB: This only works if put in the top-level CMakeLists.txt
set_directory_properties(PROPERTIES
	VS_STARTUP_PROJECT App
)

if (EMSCRIPTEN)
	# Generate a full web page rather than a simple WebAssembly module
	set_target_properties(App PROPERTIES SUFFIX ".html")
	target_link_options(App PRIVATE
		# emscripten_sleep() in requestAdapterSync/requestDeviceSync needs this
		-sASYNCIFY
		# Pack the resources folder into App.data. "@/resources" is where it
		# appears in the browser's virtual file system (RESOURCE_DIR above).
		--preload-file "${CMAKE_CURRENT_SOURCE_DIR}/resources@/resources"
	)
	# Link again (and so re-package App.data) when ONLY a resource changed.
	# Fixes the "no work to do" problem written down at Step043_cpp in MyLearnWebGPU.
	file(GLOB APP_RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
	set_property(TARGET App APPEND PROPERTY LINK_DEPENDS ${APP_RESOURCE_FILES})
endif()
