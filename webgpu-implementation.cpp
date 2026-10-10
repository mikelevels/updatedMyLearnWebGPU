// The C++ wrapper webgpu.hpp is a "header-only" library with a twist: every
// file may #include it, but exactly ONE .cpp file must #define
// WEBGPU_CPP_IMPLEMENTATION before including it, so that the wrapper's
// function bodies are compiled once (twice = "already defined" link errors,
// zero = "unresolved external symbol" link errors).
//
// REFACTOR (milestone): this used to be at the top of main.cpp. The Next guide
// keeps it in its own file (implementations.cpp), so that main.cpp, or any
// other file, can include webgpu.hpp without having to think about it.
//
// [updatedMyLearnWebGPU] The webgpu.hpp used depends on the backend we build
// with: webgpu/<backend>/include/webgpu/webgpu.hpp. The Dawn one was
// regenerated for Dawn 8093 (regenerate-dawn-hpp.bat); the wgpu-native and
// emdawnwebgpu ones are the ones WebGPU-distribution ships.
#define WEBGPU_CPP_IMPLEMENTATION
#include "webgpu/webgpu.hpp"
