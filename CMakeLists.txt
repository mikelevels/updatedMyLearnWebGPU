# updatedMyLearnWebGPU: the SAME app as MyLearnWebGPU Step039_cpp (commit 2bb72f4),
# ported to the new webgpu.h API and the latest Emscripten (checked 2026-10-08).
# It follows the "Next" table of contents of the guide:
#   https://eliemichel.github.io/LearnWebGPU/next/basic-3d-rendering/shader-uniforms/a-first-uniform.html
#   reference code: https://github.com/eliemichel/LearnWebGPU-Code/tree/step039-next
cmake_minimum_required(VERSION 3.16...3.30)
project(
	LearnWebGPU
	VERSION 0.1.0
	LANGUAGES CXX C
)

#################################################
# Versions
# Emscripten is the LATEST: 6.0.12, in C:\Users\gonza\Dev\Lib\emsdk-latest
# (see build-emscripten.bat).
# Dawn and wgpu-native stay at the versions WebGPU-distribution (the webgpu/
# folder, main @ 17dcd42) ships with, because those are the versions the
# guide's "Next" chapters are written and tested against, and the webgpu.hpp
# files inside webgpu/ were generated for exactly them:
#   Dawn chromium/7187 (prebuilt webgpu_dawn.dll), wgpu-native v24.0.3.1,
#   emdawnwebgpu v20250517.163311 (WebGPU for the browser, released by Dawn).
# Moving Dawn/wgpu-native to their latest versions means building Dawn from
# source (there is no newer prebuilt that Visual Studio 2022 can link) and
# regenerating webgpu.hpp: a separate step, for later.
# These are CACHE variables declared inside webgpu/. To try another version,
# pass -DDAWN_VERSION=... (or WGPU_VERSION, EMDAWNWEBGPU_VERSION) to a FRESH
# build folder: an existing build-* folder keeps the version it was made with.

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

if(DEV_MODE)
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
		# A preloaded FOLDER keeps its absolute path inside the browser's virtual
		# file system, so RESOURCE_DIR (DEV_MODE) still finds the files
		--preload-file "${CMAKE_CURRENT_SOURCE_DIR}/resources"
	)
	# Link again (and so re-package App.data) when ONLY a resource changed.
	# Fixes the "no work to do" problem written down at Step043_cpp in MyLearnWebGPU.
	file(GLOB APP_RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
	set_property(TARGET App APPEND PROPERTY LINK_DEPENDS ${APP_RESOURCE_FILES})
endif()
