// ApplicationPipeline.cpp: the render pipeline, i.e. HOW we draw: which
// shader, how vertices are laid out in memory, how colors are blended, the
// depth test, and which resources (bind groups) the shader expects.
//
// REFACTOR (milestone): InitializePipeline() used to fill every descriptor
// itself (170 lines). Each part is now built by a small function. Many WebGPU
// descriptors only POINT to other descriptors (e.g. the fragment state points
// to the color target, which points to the blend state), so the pieces are
// created as local variables of InitializePipeline(), which keeps them alive
// until createRenderPipeline() has read them.

#include "Application.h"
#include "ResourceManager.h"

#include <iostream>

namespace {

// How one vertex is laid out in the point buffer: x y z r g b (6 floats).
// `vertexAttribs` belongs to the caller: the layout only points to it.
wgpu::VertexBufferLayout makeVertexBufferLayout(std::vector<wgpu::VertexAttribute>& vertexAttribs) {
	vertexAttribs.assign(2, wgpu::VertexAttribute{});
	//Describe the position attribute
	vertexAttribs[0].shaderLocation = 0;//@location(0)
	vertexAttribs[0].format = wgpu::VertexFormat::Float32x3;// x, y AND z (Step050)
	vertexAttribs[0].offset = 0;
	//Describe the color attribute
	vertexAttribs[1].shaderLocation = 1;//@location(1)
	vertexAttribs[1].format = wgpu::VertexFormat::Float32x3;
	vertexAttribs[1].offset = 3*sizeof(float);// the color starts after x, y, z

	wgpu::VertexBufferLayout vertexBufferLayout = wgpu::Default;
	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
	vertexBufferLayout.attributes = vertexAttribs.data();
	vertexBufferLayout.arrayStride = 6*sizeof(float);// x y z r g b
	vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
	return vertexBufferLayout;
}

// Classic "alpha blending": new color * alpha + old color * (1 - alpha).
wgpu::BlendState makeBlendState() {
	wgpu::BlendState blendState = wgpu::Default;
	blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = wgpu::BlendOperation::Add;
	blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
	blendState.alpha.dstFactor = wgpu::BlendFactor::One;
	blendState.alpha.operation = wgpu::BlendOperation::Add;
	return blendState;
}

// The one color output of the fragment shader. Points to `blendState`.
wgpu::ColorTargetState makeColorTarget(wgpu::TextureFormat format, const wgpu::BlendState& blendState) {
	wgpu::ColorTargetState colorTarget = wgpu::Default;
	colorTarget.format = format;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = wgpu::ColorWriteMask::All;//We could write to only some of the color channels.
	return colorTarget;
}

// The fragment stage: function fs_main of the shader, one color target.
wgpu::FragmentState makeFragmentState(wgpu::ShaderModule shaderModule, const wgpu::ColorTargetState& colorTarget) {
	wgpu::FragmentState fragmentState = wgpu::Default;
	fragmentState.module = shaderModule;
	//[NEW API] entryPoint is a StringView
	fragmentState.entryPoint = wgpu::StringView("fs_main");
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;
	//We have only one target because our render pass has only one output color attachment.
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;
	return fragmentState;
}

// Vertex stage (vs_main, one vertex buffer), primitive and multisample state.
void fillVertexAndPrimitiveState(wgpu::RenderPipelineDescriptor& pipelineDesc, wgpu::ShaderModule shaderModule, const wgpu::VertexBufferLayout& vertexBufferLayout) {
	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = wgpu::StringView("vs_main");
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;
	//Each sequence of 3 vertices is considered as a triangle
	pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	// When not specified, vertices are considered sequentially.
	pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	// The face orientation is defined by assuming that when looking from the
	// front of the face, its corners are enumerated counter-clockwise (CCW).
	pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
	// We do not cull (i.e. 'hide') the faces pointing away from us.
	pipelineDesc.primitive.cullMode = wgpu::CullMode::None;
	//Samples per pixel, all bits of the mask on, no alpha-to-coverage
	pipelineDesc.multisample.count = 1;
	pipelineDesc.multisample.mask = ~0u;
	pipelineDesc.multisample.alphaToCoverageEnabled = false;
}

} // namespace

bool Application::InitializePipeline(){
	std::cout << "Creating shader module..." << std::endl;
	wgpu::ShaderModule shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
	std::cout << "Shader module: " << shaderModule << std::endl;
	if (shaderModule == nullptr) {
		std::cerr << "Could not load shader!" << std::endl;
		return false;
	}
	// The pieces the pipeline descriptor points to: they must stay alive (as
	// local variables of this function) until createRenderPipeline().
	std::vector<wgpu::VertexAttribute> vertexAttribs;
	wgpu::VertexBufferLayout vertexBufferLayout = makeVertexBufferLayout(vertexAttribs);
	wgpu::BlendState blendState = makeBlendState();
	wgpu::ColorTargetState colorTarget = makeColorTarget(surfaceFormat, blendState);
	wgpu::FragmentState fragmentState = makeFragmentState(shaderModule, colorTarget);
	wgpu::DepthStencilState depthStencilState = MakeDepthStencilState();
	CreateBindGroupLayout();
	CreatePipelineLayout();

	//[NEW API] = wgpu::Default on every descriptor. Every enum now starts with
	//Undefined = 0, and a zero-filled string means "empty string", not "not set".
	wgpu::RenderPipelineDescriptor pipelineDesc = wgpu::Default;
	pipelineDesc.label = wgpu::StringView("My render pipeline");
	fillVertexAndPrimitiveState(pipelineDesc, shaderModule, vertexBufferLayout);
	pipelineDesc.fragment = &fragmentState;
	pipelineDesc.depthStencil = &depthStencilState;
	pipelineDesc.layout = pipelineLayout;
	pipeline = device.createRenderPipeline(pipelineDesc);

	//We no longer need to access the shader module
	shaderModule.release();
	return true;
}

wgpu::DepthStencilState Application::MakeDepthStencilState() const{
	// Step052: depth testing. For each fragment, the GPU compares its depth
	// with the one already stored in the depth buffer for that pixel.
	wgpu::DepthStencilState depthStencilState = wgpu::Default;
	// Keep a fragment only if its depth is LOWER (closer) than the stored one.
	// (The default is Undefined, which the GPU treats as Always: no depth test.)
	depthStencilState.depthCompare = wgpu::CompareFunction::Less;
	// Each time a fragment is kept, store its depth so later fragments are
	// compared against it.
	//[NEW API] depthWriteEnabled is an OptionalBool now (True / False /
	//Undefined), not a plain bool: `= true` does not compile any more.
	depthStencilState.depthWriteEnabled = wgpu::OptionalBool::True;
	// Must be the same format as the depth texture (see InitializeDepthBuffer)
	depthStencilState.format = depthTextureFormat;
	// Deactivate the stencil altogether
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;
	return depthStencilState;
}

void Application::CreateBindGroupLayout(){
	// Describe the resources the shader expects (Step039): one uniform buffer
	// at @binding(0), visible to both shader stages.
	//[NEW API] An entry has 4 parts (buffer, sampler, texture, storageTexture)
	//and exactly ONE of them may be used. In the new API, "not used" is spelled
	//BindingNotUsed, and "Undefined" means "used, with the default settings".
	//The wgpu-native wrapper's wgpu::Default sets all 4 parts to Undefined,
	//i.e. USED! (The Dawn/browser wrappers fill them with 0 = BindingNotUsed.)
	//wgpu-native then saw a TEXTURE at @binding(0) and refused our pipeline
	//("does not match the pipeline binding (Texture)"). So we explicitly mark
	//the 3 parts we do not use as BindingNotUsed.
	wgpu::BindGroupLayoutEntry bindingLayout = wgpu::Default;
	bindingLayout.sampler.type = wgpu::SamplerBindingType::BindingNotUsed;
	bindingLayout.texture.sampleType = wgpu::TextureSampleType::BindingNotUsed;
	bindingLayout.storageTexture.access = wgpu::StorageTextureAccess::BindingNotUsed;
	bindingLayout.binding = 0;// @binding(0)
	bindingLayout.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindingLayout.buffer.type = wgpu::BufferBindingType::Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

	// A bind group layout is a list of binding layouts (we have only one)
	wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc = wgpu::Default;
	bindGroupLayoutDesc.entryCount = 1;
	bindGroupLayoutDesc.entries = &bindingLayout;
	bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);
}

void Application::CreatePipelineLayout(){
	// A pipeline layout is a list of bind group layouts: index 0 in this list
	// is @group(0) in the shader. We declare it explicitly (instead of letting
	// WebGPU guess it), so the bind group we create later is guaranteed to match.
	// The cast works because a wgpu::BindGroupLayout is exactly one raw
	// WGPUBindGroupLayout handle in memory.
	wgpu::PipelineLayoutDescriptor layoutDesc = wgpu::Default;
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
	pipelineLayout = device.createPipelineLayout(layoutDesc);
}
