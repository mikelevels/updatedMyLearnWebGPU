// ApplicationDevice.cpp: from nothing to a window and a working GPU device.
// Order: window -> instance + surface -> adapter -> device -> surface config.

#include "Application.h"
#include "webgpu-utils.h"// for requestAdapterSync, requestDeviceSync, toStdStringView [NEW API]

#include <glfw3webgpu.h>

#include <iostream>

namespace {

// Called by WebGPU when the device stops working (driver crash, GPU removed...
// and also when we release it on exit, so a "Device lost" line then is normal).
//[NEW API] The callback also gets the device, and the message is a WGPUStringView.
void onDeviceLost(WGPUDevice const* /* device */, WGPUDeviceLostReason reason, WGPUStringView message, void* /* userdata1 */, void* /* userdata2 */) {
	std::cout << "Device lost: reason " << reason;
	std::cout << " (" << toStdStringView(message) << ")";
	std::cout << std::endl;
}

// Called by WebGPU for every validation error that nothing else caught.
void onUncapturedError(WGPUDevice const* /* device */, WGPUErrorType type, WGPUStringView message, void* /* userdata1 */, void* /* userdata2 */) {
	std::cout << "Uncaptured device error: type " << type;
	std::cout << " (" << toStdStringView(message) << ")";
	std::cout << std::endl;
}

// Everything we ask of the device, except the limits (see RequestDevice).
wgpu::DeviceDescriptor makeDeviceDescriptor() {
	//[NEW API] = wgpu::Default instead of = {}. Zero is not a valid default for
	//everything any more (enums, strings, callback modes...).
	wgpu::DeviceDescriptor deviceDesc = wgpu::Default;
	//[NEW API] Labels are WGPUStringView now (pointer + length): a plain "..." no longer compiles
	deviceDesc.label = wgpu::StringView("My Device");
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredFeatures = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = wgpu::StringView("The default queue");
	//[NEW API] deviceLostCallback became deviceLostCallbackInfo: a callback, a mode
	//and two userdata pointers. The mode has NO default: 0 is rejected by Dawn and the browser.
	deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
	//[NEW API] The error callback is now given BEFORE the device exists, in the
	//descriptor (device.setUncapturedErrorCallback() is gone). No mode here: it
	//is an event, not a request.
	deviceDesc.uncapturedErrorCallbackInfo.callback = onUncapturedError;
	return deviceDesc;
}

//[NEW] Print WHICH GPU and backend we got. Backend type: 2 = WebGPU (the browser),
//4 = D3D12, 6 = Vulkan (see WGPUBackendType in webgpu.h)
void printAdapterInfo(wgpu::Adapter adapter) {
	wgpu::AdapterInfo adapterInfo = wgpu::Default;
	if (adapter.getInfo(&adapterInfo) == wgpu::Status::Success) {
		std::cout << "Adapter: " << toStdStringView(adapterInfo.device) << " (backend type " << adapterInfo.backendType << ")" << std::endl;
		//The strings inside adapterInfo were allocated by WebGPU, give them back
		adapterInfo.freeMembers();
	}
}

//[NEW] Print the limits we really got (requiredLimits only means "at least")
void printDeviceLimits(wgpu::Device device) {
	wgpu::Limits deviceLimits = wgpu::Default;
	if (device.getLimits(&deviceLimits) == wgpu::Status::Success) {
		std::cout << "Device limits: maxVertexAttributes " << deviceLimits.maxVertexAttributes
			<< ", maxBufferSize " << deviceLimits.maxBufferSize
			<< ", maxUniformBufferBindingSize " << deviceLimits.maxUniformBufferBindingSize << std::endl;
	}
}

} // namespace

bool Application::InitializeWindow(){
	if (!glfwInit()) {
		std::cerr << "Could not initialize GLFW!" << std::endl;
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
	if (!window) {
		std::cerr << "Could not open window!" << std::endl;
		return false;
	}
	return true;
}

bool Application::InitializeDevice(){
	if (!CreateInstanceAndSurface()) return false;
	wgpu::Adapter adapter = RequestAdapter();
	if (!adapter) return false;
	printAdapterInfo(adapter);
	bool success = RequestDevice(adapter) && ConfigureSurface(adapter);
	// Release the adapter only after it has been fully utilized
	adapter.release();
	return success;
}

bool Application::CreateInstanceAndSurface(){
	//[NEW API] `instance` is a class member now (see Application.h).
	//The "descriptor must be nullptr in the browser" rule is gone too: it only
	//existed for the old -sUSE_WEBGPU, which emscripten removed in 4.0.18.
	instance = wgpuCreateInstance(nullptr);
	if (!instance) {
		std::cerr << "Could not create a WebGPU instance!" << std::endl;
		return false;
	}
	//[NEW API] glfwGetWGPUSurface() is called glfwCreateWindowWGPUSurface() since glfw3webgpu v1.3.0-alpha
	surface = glfwCreateWindowWGPUSurface(instance, window);
	if (!surface) {
		std::cerr << "Could not create the window surface!" << std::endl;
		return false;
	}
	return true;
}

wgpu::Adapter Application::RequestAdapter(){
	std::cout << "Requesting adapter..." << std::endl;
	wgpu::RequestAdapterOptions adapterOpts = wgpu::Default;
	adapterOpts.compatibleSurface = surface;
	//[NEW API] instance.requestAdapter(adapterOpts) is replaced by requestAdapterSync()
	//(webgpu-utils.cpp). Every request is asynchronous now, the helper waits for the
	//answer by calling instance.processEvents() in a loop.
	wgpu::Adapter adapter = requestAdapterSync(instance, &adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;
	if (!adapter) {
		std::cerr << "Could not get a WebGPU adapter!" << std::endl;
	}
	return adapter;
}

bool Application::RequestDevice(wgpu::Adapter adapter){
	std::cout << "Requesting device..." << std::endl;
	wgpu::DeviceDescriptor deviceDesc = makeDeviceDescriptor();
	// The descriptor only POINTS to the limits, so they must stay alive until
	// requestDeviceSync() returns: a local variable of THIS function is fine.
	wgpu::Limits requiredLimits = GetRequiredLimits(adapter);
	deviceDesc.requiredLimits = &requiredLimits;
	//[NEW API] adapter.requestDevice(deviceDesc) is replaced by requestDeviceSync() (webgpu-utils.cpp)
	device = requestDeviceSync(instance, adapter, &deviceDesc);
	std::cout << "Got device: " << device << std::endl;
	if (!device) {
		// This is exactly what used to happen in the browser at Step037: Chrome
		// refused one of our required limits, so no device was created.
		std::cerr << "Could not get a WebGPU device!" << std::endl;
		return false;
	}
	printDeviceLimits(device);
	queue = device.getQueue();
	return true;
}

bool Application::ConfigureSurface(wgpu::Adapter adapter){
	//[NEW API] surface.getPreferredFormat(adapter) is gone. We ask for the surface
	//CAPABILITIES instead: the preferred format is always the first one in the list.
	wgpu::SurfaceCapabilities capabilities = wgpu::Default;
	if (surface.getCapabilities(adapter, &capabilities) != wgpu::Status::Success || capabilities.formatCount == 0) {
		std::cerr << "Could not get the surface capabilities!" << std::endl;
		return false;
	}
	surfaceFormat = capabilities.formats[0];
	//freeMembers() gives the formats array back to WebGPU, so copy formats[0] BEFORE this line
	capabilities.freeMembers();

	// Configuration of the textures created for the underlying swap chain.
	// (surfaceConfig is a member: GetNextSurfaceTextureView() needs it to
	// configure the surface again when it becomes "Outdated" or "Lost")
	surfaceConfig = wgpu::Default;
	surfaceConfig.width = 640;
	surfaceConfig.height = 480;
	surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
	surfaceConfig.format = surfaceFormat;
	// And we do not need any particular view format:
	surfaceConfig.viewFormatCount = 0;
	surfaceConfig.viewFormats = nullptr;
	surfaceConfig.device = device;
	surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
	surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
	surface.configure(surfaceConfig);
	return true;
}

wgpu::Limits Application::GetRequiredLimits(wgpu::Adapter adapter) const{
	//[NEW API] SupportedLimits and RequiredLimits are gone: both are a plain wgpu::Limits
	//now (no more `.limits.` in the middle), and getLimits() returns a wgpu::Status
	wgpu::Limits supportedLimits = wgpu::Default;
	if (adapter.getLimits(&supportedLimits) != wgpu::Status::Success) {
		std::cerr << "Could not get the adapter limits!" << std::endl;
	}

	//Don't forget to = Default
	//[NEW API] Default fills EVERY limit with WGPU_LIMIT_U32_UNDEFINED or
	//WGPU_LIMIT_U64_UNDEFINED, which means "use the WebGPU default value".
	wgpu::Limits requiredLimits = wgpu::Default;

	//[NEW] We no longer ask for the SMALLEST limits our app fits in. MyLearnWebGPU had:
	//	maxVertexAttributes = 2, maxVertexBuffers = 1, maxBufferSize = 15*5*sizeof(float),
	//	maxVertexBufferArrayStride = 5*sizeof(float), maxBindGroups = 1,
	//	maxUniformBuffersPerShaderStage = 1, maxUniformBufferBindingSize = 16*4,
	//	maxInterStageShaderComponents = 3 (that one does not even EXIST any more).
	//Values below the default are IGNORED by Dawn and Chrome (you always get at least
	//the default) but wgpu-native really enforces them, so the 3 builds would not
	//behave the same. The Next guide leaves every limit at Default and only RAISES
	//one when a chapter needs more. Our app is far below every default
	//(e.g. maxUniformBufferBindingSize is 65536 bytes, we use
	//sizeof(MyUniforms) = 224).

	// These two are "alignment" limits, where LOWER is better. Left at
	// Default the device gets 256; forwarding the adapter's value lets us use
	// the finer alignment it supports. (Any value the adapter supports may be
	// asked for; for max* limits that would just ask for more than we need.)
	requiredLimits.minUniformBufferOffsetAlignment = supportedLimits.minUniformBufferOffsetAlignment;
	requiredLimits.minStorageBufferOffsetAlignment = supportedLimits.minStorageBufferOffsetAlignment;
	return requiredLimits;
}
