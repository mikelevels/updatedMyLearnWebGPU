// Application.cpp: the four public functions. Each one reads like a table of
// contents: it only calls the substeps, in order, and the substeps live in
// the topic files (ApplicationDevice.cpp, ApplicationPipeline.cpp,
// ApplicationResources.cpp, ApplicationFrame.cpp).

#include "Application.h"

bool Application::Initialize(){
	// Every step below can fail (no GPU, driver too old, browser without
	// WebGPU...). Each substep checks its own result and prints a clear
	// message, and we stop at the first failure.
	// These substeps return false instead of calling exit(1), so main()
	// decides what to do on failure. NOTE: main() currently just returns 1
	// without calling Terminate(), so nothing is released on this path (the
	// OS frees everything when the program exits). Terminate() is not safe
	// after a partial init yet: it would release null handles.
	if (!InitializeWindow()) return false;
	if (!InitializeDevice()) return false;
	if (!InitializePipeline()) return false;
	InitializeDepthBuffer();
	if (!InitializeBuffers()) return false;
	InitializeBindGroups();
	return true;
}

void Application::Terminate(){
	// Release in the reverse order of creation: things that USE an object are
	// released before the object they use.
	// (WebGPU objects are reference counted, so a different order would not
	// crash, but this order is easy to reason about.)
	bindGroup.release();
	uniformBuffer.release();
	indexBuffer.release();
	pointBuffer.release();
	// Step052: the view first, then the texture. destroy() frees the GPU
	// memory right away; release() drops our handle to the object.
	depthTextureView.release();
	depthTexture.destroy();
	depthTexture.release();
	pipeline.release();
	pipelineLayout.release();
	bindGroupLayout.release();
	surface.unconfigure();
	queue.release();
	surface.release();
	//[NEW API] Releasing the last reference to the device now DESTROYS it, so a
	//"Device lost" line at exit is normal.
	device.release();
	//[NEW API] The instance lives until the very end now (see CreateInstanceAndSurface())
	instance.release();
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::MainLoop(){
	glfwPollEvents();
	//[NEW API] Let WebGPU run the callbacks that are ready (device lost...). This ONE
	//line replaces the old #if block (device.tick() for Dawn, device.poll(false)
	//for wgpu-native), and it works on all 3 builds.
	instance.processEvents();

	// Get the next target texture view (nullptr = skip this frame)
	wgpu::TextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;

	UpdateUniforms();

	// Record the drawing commands, then send them to the GPU
	wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
	encoderDesc.label = wgpu::StringView("My command encoder");
	wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);
	RecordRenderPass(encoder, targetView);
	SubmitCommands(encoder);

	PresentFrame(targetView);
}

bool Application::IsRunning(){
	return !glfwWindowShouldClose(window);
}
