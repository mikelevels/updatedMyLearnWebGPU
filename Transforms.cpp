#include "Transforms.h"

#include <cmath>

// glm::mat4x4 takes its 16 numbers column by column, like WGSL's mat4x4f. We
// write each matrix ROW by row (the way it looks on paper) and wrap it in
// glm::transpose(), the same trick as in Step054's shader.

mat4x4 makeScaleByHand(float factor){
	return glm::transpose(mat4x4(
		factor, 0.0, 0.0, 0.0,
		0.0, factor, 0.0, 0.0,
		0.0, 0.0, factor, 0.0,
		0.0, 0.0, 0.0, 1.0
	));
}

mat4x4 makeTranslationByHand(vec3 offset){
	// A 3x3 matrix can only MIX coordinates, it cannot ADD a constant. With a
	// 4th coordinate w that is always 1.0, the last column adds offset * 1.0.
	return glm::transpose(mat4x4(
		1.0, 0.0, 0.0, offset.x,
		0.0, 1.0, 0.0, offset.y,
		0.0, 0.0, 1.0, offset.z,
		0.0, 0.0, 0.0, 1.0
	));
}

mat4x4 makeRotationZByHand(float angle){
	// Sanity check: at angle = 0, c = 1 and s = 0: the identity matrix.
	// Rows (c, s) and (-s, c) send the x axis to (c, -s): a rotation by
	// MINUS angle. glm::rotate turns by PLUS angle, which is why the GLM
	// versions are given -angle (bug review fix).
	float c = std::cos(angle);
	float s = std::sin(angle);
	return glm::transpose(mat4x4(
		  c,   s, 0.0, 0.0,
		 -s,   c, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	));
}

mat4x4 makeRotationXByHand(float angle){
	float c = std::cos(angle);
	float s = std::sin(angle);
	return glm::transpose(mat4x4(
		1.0, 0.0, 0.0, 0.0,
		0.0,   c,   s, 0.0,
		0.0,  -s,   c, 0.0,
		0.0, 0.0, 0.0, 1.0
	));
}

mat4x4 makePerspectiveByHand(float focalLength, float ratio, float near, float far){
	// The key is the last row (0, 0, 1, 0): it makes w = z, and the GPU then
	// DIVIDES x, y and z by w. Far things (big z) get divided more, so they
	// look SMALLER. That is perspective.
	// The third row maps z from [near, far] to the depth range [0, 1]
	// (replacing the "z * 0.5 + 0.5" trick of Step052).
	// BUG FIX (bug review): the guide's matrix scaled x by 1 and y by ratio
	// with w = z / focalLength, which is NOT what glm::perspective builds (the
	// GLM picture was 25% smaller). This is exactly glm::perspective's matrix:
	// x scaled by focalLength / ratio, y by focalLength.
	float divider = 1 / (far - near);
	return glm::transpose(mat4x4(
		focalLength / ratio, 0.0, 0.0, 0.0,
		0.0, focalLength, 0.0, 0.0,
		0.0, 0.0, far * divider, -far * near * divider,
		0.0, 0.0, 1.0, 0.0
	));
}
