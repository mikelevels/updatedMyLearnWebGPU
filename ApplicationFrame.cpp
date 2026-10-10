// ApplicationFrame.cpp: everything that happens once per frame, in the order
// MainLoop() calls it: get the surface texture, update the uniforms, record
// the render pass, submit, present.

#include "Application.h"

#include <cmath>// for std::pow

wgpu::TextureView Application::GetNextSurfaceTextureView(){
	//[NEW API] = wgpu::Default (the old code left this struct uninitialized)
	wgpu::SurfaceTexture surfaceTexture = wgpu::Default;
	surface.getCurrentTexture(&surfaceTexture);
	//[NEW API] There is no plain "Success" any more. SuccessOptimal is the normal
	//case, SuccessSuboptimal still gives a texture we can draw into (the surface
	//would just like to be configured again, e.g. after a resize).
	if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal
		&& surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
		if (surfaceTexture.texture != nullptr) {
			wgpuTextureRelease(surfaceTexture.texture);
		}
		RecoverSurface(surfaceTexture.status);
		return nullptr;
	}
	//[NEW API] Keep the texture until the END of the frame (see PresentFrame). The
	//Next guide releases it right after createView(), but that discards the frame
	//on wgpu-native.
	currentSurfaceTexture = surfaceTexture.texture;

	// Create a view for this surface texture
	wgpu::TextureViewDescriptor viewDescriptor = wgpu::Default;
	viewDescriptor.label = wgpu::StringView("Surface texture view");
	viewDescriptor.format = currentSurfaceTexture.getFormat();
	viewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = wgpu::TextureAspect::All;
	return currentSurfaceTexture.createView(viewDescriptor);
}

void Application::RecoverSurface(wgpu::SurfaceGetCurrentTextureStatus status){
	//BUG FIX (bug review): before, we skipped the frame and never did anything
	//else. An "Outdated" or "Lost" surface (e.g. after minimizing and restoring
	//the window, on wgpu-native/Vulkan) then stayed unusable forever and the
	//window froze. Configuring it again makes it usable.
	if (status != wgpu::SurfaceGetCurrentTextureStatus::Outdated
		&& status != wgpu::SurfaceGetCurrentTextureStatus::Lost) {
		return;
	}
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	if (width > 0 && height > 0) {// 0 x 0 while minimized: wait
		surface.configure(surfaceConfig);
	}
}

void Application::UpdateUniforms(){
	// We only overwrite the fields that change: `offsetof` gives where each
	// one sits inside MyUniforms, so the other fields keep their values.
	uniforms.time = static_cast<float>(glfwGetTime());
	queue.writeBuffer(uniformBuffer, offsetof(MyUniforms, time), &uniforms.time, sizeof(MyUniforms::time));

	// Step055: the spin of the pyramid is done on the CPU. We rebuild the
	// model matrix with the new angle (rotation around Z) and upload ONLY
	// that matrix (64 bytes). Read right to left: scale, translate, rotate.
	// (Minus angle: see BuildMatricesWithGlm in ApplicationResources.cpp.)
	float angle1 = uniforms.time;
	mat4x4 R1 = glm::rotate(mat4x4(1.0), -angle1, vec3(0.0, 0.0, 1.0));
	uniforms.modelMatrix = R1 * modelTranslation * modelScale;
	queue.writeBuffer(uniformBuffer, offsetof(MyUniforms, modelMatrix), &uniforms.modelMatrix, sizeof(MyUniforms::modelMatrix));
}

void Application::RecordRenderPass(wgpu::CommandEncoder encoder, wgpu::TextureView targetView){
	// The descriptor only POINTS to the two attachments, so they are local
	// variables of this function, alive until beginRenderPass() has read them.
	wgpu::RenderPassColorAttachment colorAttachment = MakeColorAttachment(targetView);
	wgpu::RenderPassDepthStencilAttachment depthAttachment = MakeDepthAttachment();

	wgpu::RenderPassDescriptor renderPassDesc = wgpu::Default;
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &colorAttachment;
	renderPassDesc.depthStencilAttachment = &depthAttachment;
	renderPassDesc.timestampWrites = nullptr;

	wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
	DrawScene(renderPass);
	renderPass.end();
	renderPass.release();
}

wgpu::RenderPassColorAttachment Application::MakeColorAttachment(wgpu::TextureView targetView) const{
	// The color attachment describes the target texture of the pass
	wgpu::RenderPassColorAttachment colorAttachment = wgpu::Default;
	colorAttachment.view = targetView;
	colorAttachment.resolveTarget = nullptr;
	colorAttachment.loadOp = wgpu::LoadOp::Clear;
	colorAttachment.storeOp = wgpu::StoreOp::Store;
	// The clear color goes through the same sRGB conversion as our shader's
	// output, so it needs the same gamma correction to look identical on
	// every backend (otherwise it is near-black in the browser and grey with
	// wgpu-native). NB: pow 2.2 is only CLOSE to the real sRGB curve; near
	// black they still differ by a few levels (about 13 vs 5 out of 255 here).
	double background = std::pow(0.05, gamma);
	colorAttachment.clearValue = WGPUColor{ background, background, background, 1.0 };
	//[NEW API] depthSlice exists on every backend now, so the old
	//#ifndef WEBGPU_BACKEND_WGPU around it is gone. The Dawn/browser wrapper's
	//wgpu::Default already puts this value here, but the wgpu-native wrapper
	//leaves 0, so this line is REQUIRED.
	colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
	return colorAttachment;
}

wgpu::RenderPassDepthStencilAttachment Application::MakeDepthAttachment() const{
	// Step052: the depth/stencil attachment, next to the color one.
	wgpu::RenderPassDepthStencilAttachment depthAttachment = wgpu::Default;
	depthAttachment.view = depthTextureView;
	// The initial value of the depth buffer: 1.0 means "as far as possible",
	// so the first fragment drawn on each pixel always passes the test.
	// (wgpu::Default does NOT set a usable value: the wgpu-native wrapper
	// leaves 0.0 = "nearest", so nothing would be drawn; Dawn and the browser
	// leave NaN = "undefined", a validation error with LoadOp::Clear.)
	depthAttachment.depthClearValue = 1.0f;
	// Clear at the start of the pass, keep the result at the end
	depthAttachment.depthLoadOp = wgpu::LoadOp::Clear;
	depthAttachment.depthStoreOp = wgpu::StoreOp::Store;
	depthAttachment.depthReadOnly = false;
	// Stencil: mandatory fields, but we do not use a stencil. Depth24Plus has
	// NO stencil part, so the stencil ops must be Undefined on every backend.
	depthAttachment.stencilClearValue = 0;
	depthAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
	depthAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
	depthAttachment.stencilReadOnly = true;
	return depthAttachment;
}

void Application::DrawScene(wgpu::RenderPassEncoder renderPass){
	//Select which render pipeline to use
	renderPass.setPipeline(pipeline);
	//Set POINT buffer while encoding the render pass
	renderPass.setVertexBuffer(0, pointBuffer, 0, pointBuffer.getSize());
	//The second argument must match the type of our index data: uint16_t
	//here (Uint16), it would be Uint32 for a std::vector<uint32_t>
	renderPass.setIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16, 0, indexBuffer.getSize());
	//Plug our bind group into slot @group(0) of the shader, so that uMyUniforms
	//reads from uniformBuffer. (The last two arguments are for "dynamic
	//offsets", which we do not use.)
	renderPass.setBindGroup(0, bindGroup, 0, nullptr);
	//The extra argument is an offset within the index buffer.
	renderPass.drawIndexed(indexCount, 1, 0, 0, 0);
}

void Application::SubmitCommands(wgpu::CommandEncoder encoder){
	wgpu::CommandBufferDescriptor cmdBufferDescriptor = wgpu::Default;
	cmdBufferDescriptor.label = wgpu::StringView("Command buffer");
	wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();
	// No logging here: this runs ~60 times per second, and printing every
	// frame floods the console and slows the app down.
	queue.submit(1, &command);
	command.release();
}

void Application::PresentFrame(wgpu::TextureView targetView){
	targetView.release();
#ifndef __EMSCRIPTEN__
	//(present() returns a wgpu::Status now, we do not need it here)
	surface.present();
#endif // NOT __EMSCRIPTEN__
	//[NEW API] Release the surface texture AFTER present(). wgpu-native throws the
	//frame away if its texture is released before present() (MyLearnWebGPU never
	//released it at all, which leaked one texture reference per frame).
	currentSurfaceTexture.release();
	currentSurfaceTexture = nullptr;
}
