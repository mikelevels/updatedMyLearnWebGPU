#pragma once
/**
 * Transforms.h: GLM, plus the matrices of Step054/Step055 written BY HAND.
 *
 * REFACTOR (milestone): "Option A" of Step055 used to build five matrices in
 * the middle of InitializeBuffers(). Each one is now a small named function,
 * so you can read the math of ONE transform at a time, and test it alone.
 */

// GLM ("OpenGL Mathematics", Step055): vectors and matrices on the C++ side,
// with the same names and layout as in shaders (vec3, vec4, mat4x4...).
// It is header-only: the glm/ folder at the root of the project is all of it.
// The two defines MUST come before the include:
// - DEPTH_ZERO_TO_ONE: WebGPU keeps depths between 0 and 1 (OpenGL, which
//   GLM was made for, uses -1 to 1).
// - LEFT_HANDED: x to the right, y up, z going INTO the screen, like WebGPU's
//   clip space. (OpenGL's z comes out of the screen.)
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp> // all types inspired from GLSL
#include <glm/ext.hpp> // glm::perspective, glm::rotate, glm::translate...

using glm::mat4x4;
using glm::vec4;
using glm::vec3;

constexpr float PI = 3.14159265358979323846f;

// Scale every axis by `factor`: the diagonal holds the factors.
mat4x4 makeScaleByHand(float factor);

// Move by `offset`: the 4th column adds offset * w, with w = 1.0
// ("homogeneous coordinates", Step054).
mat4x4 makeTranslationByHand(vec3 offset);

// Rotate in the XY plane (around Z). Because of the transpose() trick this is
// a rotation by MINUS `angle`, like in Step054's shader (see Transforms.cpp).
mat4x4 makeRotationZByHand(float angle);

// Rotate in the YZ plane (around X), also by MINUS `angle`.
mat4x4 makeRotationXByHand(float angle);

// Perspective projection, identical to glm::perspective(2 * atan(1 / focalLength),
// ratio, near, far) with our two GLM defines (see Transforms.cpp).
mat4x4 makePerspectiveByHand(float focalLength, float ratio, float near, float far);
