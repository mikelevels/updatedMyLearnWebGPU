// ApplicationResources.cpp: WHAT we draw. The geometry buffers, the uniform
// buffer and its initial values (the three transform matrices and the
// gamma), the depth texture and the bind group.

#include "Application.h"
#include "ResourceManager.h"

#include <iostream>

namespace {

// The camera and object settings shared by the three ways of building the
// matrices (Options A, B and C of Step055).
const float kScale = 0.3f;                       // size of the pyramid
const vec3 kObjectOffset = vec3(0.5, 0.0, 0.0);  // where it orbits
const float kStartAngle = 2.0f;                  // arbitrary time; UpdateUniforms() replaces it
const float kViewTilt = 3.0f * PI / 4.0f;        // three 8th of a turn
// Moving the CAMERA back to z = -2 is the same as moving the whole world
// forward by +2, hence the minus sign where it is used.
const vec3 kFocalPoint = vec3(0.0, 0.0, -2.0);
const float kRatio = 640.0f / 480.0f;
const float kFocalLength = 2.0f;
const float kNear = 0.01f;
const float kFar = 100.0f;

} // namespace

bool Application::InitializeBuffers(){
	// Step050: load the 3D pyramid: 6 floats per vertex (x, y, z then r, g, b)
	// and a list of indices referencing positions in pointData. The last
	// argument tells loadGeometry how many position coordinates a line has.
	std::vector<float> pointData;
	std::vector<uint16_t> indexData;
	bool success = ResourceManager::loadGeometry(RESOURCE_DIR "/pyramid.txt", pointData, indexData, 3 /* dimensions */);
	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
		return false;
	}
	indexCount = static_cast<uint32_t>(indexData.size());

	// writeBuffer only accepts sizes that are a multiple of 4 bytes, but each
	// index is 2 bytes. With an odd number of indices (15 in webgpu.txt, the
	// pyramid has 18 so it does not need it, but the next model might) we
	// would copy 2 bytes past the end of indexData, i.e. read memory that does
	// not belong to us. Adding a dummy 0 index makes the vector itself long
	// enough. It is never drawn, because drawIndexed uses indexCount (saved
	// above, before the padding).
	if (indexData.size() % 2 != 0) {
		indexData.push_back(0);
	}
	CreateGeometryBuffers(pointData, indexData);
	CreateUniformBuffer();
	InitializeUniforms();
	return true;
}

void Application::CreateGeometryBuffers(const std::vector<float>& pointData, const std::vector<uint16_t>& indexData){
	//Create a POINT buffer
	wgpu::BufferDescriptor bufferDesc = wgpu::Default;
	//[NEW] A label shows up in error messages (Next guide, "Dealing with errors")
	bufferDesc.label = wgpu::StringView("Point buffer");
	bufferDesc.size = pointData.size()*sizeof(float);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;// Vertex usage here!
	bufferDesc.mappedAtCreation = false;
	pointBuffer = device.createBuffer(bufferDesc);
	//Upload geometry data to the buffer
	queue.writeBuffer(pointBuffer, 0, pointData.data(), bufferDesc.size);

	//Create index buffer (we reuse the bufferDesc initialized for the pointBuffer)
	bufferDesc.label = wgpu::StringView("Index buffer");
	bufferDesc.size = indexData.size()*sizeof(uint16_t);// already a multiple of 4, see padding
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
	indexBuffer = device.createBuffer(bufferDesc);
	queue.writeBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);
}

void Application::CreateUniformBuffer(){
	//It holds exactly one MyUniforms struct.
	wgpu::BufferDescriptor bufferDesc = wgpu::Default;
	bufferDesc.label = wgpu::StringView("Uniform buffer");
	bufferDesc.size = sizeof(MyUniforms);
	//Make sure to flag the buffer as BufferUsage::Uniform
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	uniformBuffer = device.createBuffer(bufferDesc);
}

void Application::InitializeUniforms(){
	//(`uniforms` is a member: UpdateUniforms() updates it every frame)
	uniforms = {};
	uniforms.time = 0.0f;
	// A green tint, as in the guide (RGBA, each between 0 and 1). The shader
	// multiplies every vertex color by it, so the white pyramid base looks green.
	uniforms.color = { 0.0f, 1.0f, 0.4f, 1.0f };

	// Step055: build the transform matrices ONCE here on the CPU, instead of
	// for every single vertex in the shader (that was Step054). The guide
	// shows three ways of building the same matrices; each one overwrites
	// the previous, Option C is the one that is used.
	// The three matrices, applied right to left to each vertex:
	//   projection * view * model * position
	//   - model: where the object sits in the world (scale, move, spin)
	//   - view: where the camera is and where it looks
	//   - projection: how the 3D view is flattened onto the screen
	BuildMatricesByHand();
	BuildMatricesWithGlm();
	BuildMatricesWithGlmChain();
	ChooseGamma();
	queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(MyUniforms));
}

void Application::BuildMatricesByHand(){
	// --- Option A: write the matrices by hand, exactly like in Step054's
	// shader (see Transforms.cpp for each one).
	mat4x4 S = makeScaleByHand(kScale);
	mat4x4 T1 = makeTranslationByHand(kObjectOffset);
	mat4x4 R1 = makeRotationZByHand(kStartAngle);
	mat4x4 T2 = makeTranslationByHand(-kFocalPoint);
	mat4x4 R2 = makeRotationXByHand(kViewTilt);
	uniforms.modelMatrix = R1 * T1 * S;
	uniforms.viewMatrix = T2 * R2;
	uniforms.projectionMatrix = makePerspectiveByHand(kFocalLength, kRatio, kNear, kFar);
}

void Application::BuildMatricesWithGlm(){
	// --- Option B: let GLM build each matrix for us.
	mat4x4 S = glm::scale(mat4x4(1.0), vec3(kScale));
	mat4x4 T1 = glm::translate(mat4x4(1.0), kObjectOffset);
	// BUG FIX (bug review): the hand-written rotations are transposed, so they
	// turn by MINUS the angle (like Step054's shader). glm::rotate turns by
	// PLUS the angle, so we give it -angle to get the same matrix. Before,
	// R1 was not compensated and the pyramid spun the other way.
	mat4x4 R1 = glm::rotate(mat4x4(1.0), -kStartAngle, vec3(0.0, 0.0, 1.0));
	uniforms.modelMatrix = R1 * T1 * S;

	mat4x4 R2 = glm::rotate(mat4x4(1.0), -kViewTilt, vec3(1.0, 0.0, 0.0));
	mat4x4 T2 = glm::translate(mat4x4(1.0), -kFocalPoint);
	uniforms.viewMatrix = T2 * R2;
	// Keep the parts of the model matrix that never change, for UpdateUniforms()
	modelScale = S;
	modelTranslation = T1;
}

void Application::BuildMatricesWithGlmChain(){
	// --- Option C: chain GLM calls on one matrix. CAREFUL: each call
	// multiplies on the RIGHT, so the calls are written in the OPPOSITE order
	// of what happens to the vertex (rotate is written first but applied last).
	mat4x4 M(1.0);
	M = glm::rotate(M, -kStartAngle, vec3(0.0, 0.0, 1.0));// minus: see Option B
	M = glm::translate(M, kObjectOffset);
	M = glm::scale(M, vec3(kScale));
	uniforms.modelMatrix = M;

	mat4x4 V(1.0);
	V = glm::translate(V, -kFocalPoint);
	V = glm::rotate(V, -kViewTilt, vec3(1.0, 0.0, 0.0));
	uniforms.viewMatrix = V;

	// glm::perspective takes a vertical field of view instead of a focal
	// length: a focal length of 2 means seeing 1 unit up for 2 units forward.
	float fov = 2 * glm::atan(1 / kFocalLength);
	uniforms.projectionMatrix = glm::perspective(fov, kRatio, kNear, kFar);
}

void Application::ChooseGamma(){
	// Gamma correction. Our colors in the .txt files are written in sRGB (the
	// space color pickers use). If the surface is an *sRGB* format, the GPU
	// converts linear -> sRGB when writing pixels, so we must first convert
	// our sRGB colors to linear (pow 2.2) or they come out washed out.
	// If the surface is a plain (non-sRGB) format, nothing is converted and we
	// must leave the colors alone (pow 1.0). Which one we get depends on the
	// backend: wgpu-native usually picks BGRA8UnormSrgb, Dawn and browsers
	// usually BGRA8Unorm. This is why your Step025 note saw different colors.
	bool isSrgb = surfaceFormat == wgpu::TextureFormat::BGRA8UnormSrgb
		|| surfaceFormat == wgpu::TextureFormat::RGBA8UnormSrgb;
	gamma = isSrgb ? 2.2f : 1.0f;
	uniforms.gamma = gamma;
	//The number printed is the enum's value in webgpu.h, and webgpu.h versions
	//RENUMBER the formats (BGRA8Unorm is 23 with Dawn 7187 / wgpu-native v24 /
	//the browser, 27 with Dawn 8093). Compare with the names, never the numbers.
	std::cout << "Surface format: " << surfaceFormat << " (sRGB: " << (isSrgb ? "yes" : "no") << ")" << std::endl;
}

void Application::InitializeDepthBuffer(){
	// Create the depth texture (Step052). It must be exactly the size of the
	// surface we draw to: 640 x 480. (Hard-coded for now, like the window. It
	// will have to be re-created when we allow resizing.)
	wgpu::TextureDescriptor depthTextureDesc = wgpu::Default;
	depthTextureDesc.label = wgpu::StringView("Depth texture");
	depthTextureDesc.dimension = wgpu::TextureDimension::_2D;
	depthTextureDesc.format = depthTextureFormat;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = {640, 480, 1};
	// The only thing we do with it is use it as a render pass attachment
	depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&depthTextureFormat;
	depthTexture = device.createTexture(depthTextureDesc);

	// A texture can hold many images (mip levels, array layers) and a view
	// says which part we use. Here: the whole texture, depth part only.
	wgpu::TextureViewDescriptor depthTextureViewDesc = wgpu::Default;
	depthTextureViewDesc.label = wgpu::StringView("Depth texture view");
	depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
	depthTextureViewDesc.format = depthTextureFormat;
	depthTextureView = depthTexture.createView(depthTextureViewDesc);
}

void Application::InitializeBindGroups(){
	// The bind group is where we say WHICH buffer goes into each binding
	// declared by bindGroupLayout. One entry per binding.
	wgpu::BindGroupEntry binding = wgpu::Default;
	binding.binding = 0;// Must match bindingLayout.binding and @binding(0)
	binding.buffer = uniformBuffer;
	binding.offset = 0;// We use the buffer from its very beginning...
	binding.size = sizeof(MyUniforms);// ...up to the size of our struct

	wgpu::BindGroupDescriptor bindGroupDesc = wgpu::Default;
	bindGroupDesc.label = wgpu::StringView("Uniform bind group");
	bindGroupDesc.layout = bindGroupLayout;// The bind group must follow this layout
	bindGroupDesc.entryCount = 1;
	bindGroupDesc.entries = &binding;
	bindGroup = device.createBindGroup(bindGroupDesc);
}
