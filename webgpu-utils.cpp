#include "webgpu-utils.h"

#include <iostream>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#else
    #include <thread>
    #include <chrono>
#endif // __EMSCRIPTEN__

std::string_view toStdStringView(WGPUStringView wgpuStringView){
	//{nullptr, ...} means "no string at all"
	if (wgpuStringView.data == nullptr) {
		return std::string_view();
	}
	//WGPU_STRLEN means "it ends with '\0', count the characters yourself"
	if (wgpuStringView.length == WGPU_STRLEN) {
		return std::string_view(wgpuStringView.data);
	}
	//Otherwise the length is given and there may be NO '\0' at the end!
	return std::string_view(wgpuStringView.data, wgpuStringView.length);
}

void sleepForMilliseconds(unsigned int milliseconds){
#ifdef __EMSCRIPTEN__
	//Gives control back to the browser. Only allowed because we link with -sASYNCIFY
	emscripten_sleep(milliseconds);
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif // __EMSCRIPTEN__
}

// REFACTOR (milestone): requestAdapterSync and requestDeviceSync used to have
// the same waiting loop copied in each. It now lives here, once.
// We wait until `requestEnded` becomes true (the callback sets it).
// processEvents() lets the instance run the callbacks that are ready.
// NB: We test once before the loop not to wait in case it is already ready
// (wgpu-native even calls the callback inside the request function).
static void waitUntilRequestEnded(WGPUInstance instance, const bool& requestEnded){
	wgpuInstanceProcessEvents(instance);
	while (!requestEnded) {
		// Wait a little to avoid asking too often (the guide waits 200 ms)
		sleepForMilliseconds(20);
		wgpuInstanceProcessEvents(instance);
	}
}

WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options){
	// A simple structure holding the local information shared with the
	// onAdapterRequestEnded callback.
	struct UserData {
		WGPUAdapter adapter = nullptr;
		bool requestEnded = false;
	};
	UserData userData;

	// Callback called by wgpuInstanceRequestAdapter when the request returns.
	// It must be non-capturing (the brackets [] are empty) so that it behaves
	// like a regular C function pointer, which is what WebGPU (a C API) expects.
	// What we want to "capture" travels through the userdata1 pointer instead.
	auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* /* userdata2 */) {
		UserData& data = *reinterpret_cast<UserData*>(userdata1);
		if (status == WGPURequestAdapterStatus_Success) {
			data.adapter = adapter;
		}
		else {
			std::cerr << "Error while requesting adapter: " << toStdStringView(message) << std::endl;
		}
		data.requestEnded = true;
	};

	//Build the callback info. `= {}` sets every field to zero/nullptr (newer
	//headers have a WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT macro for this, the
	//wgpu-native v24 header does not). The mode has NO valid default value: 0 is
	//rejected by Dawn and the browser ("Invalid callback mode"), so we set it.
	WGPURequestAdapterCallbackInfo callbackInfo = {};
	callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	callbackInfo.callback = onAdapterRequestEnded;
	callbackInfo.userdata1 = &userData;
	callbackInfo.userdata2 = nullptr;

	// Call to the WebGPU request adapter procedure
	wgpuInstanceRequestAdapter(instance, options, callbackInfo);

	waitUntilRequestEnded(instance, userData.requestEnded);

	return userData.adapter;
}

WGPUDevice requestDeviceSync(WGPUInstance instance, WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor){
	//Same idea as requestAdapterSync, see the comments there
	struct UserData {
		WGPUDevice device = nullptr;
		bool requestEnded = false;
	};
	UserData userData;

	auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* /* userdata2 */) {
		UserData& data = *reinterpret_cast<UserData*>(userdata1);
		if (status == WGPURequestDeviceStatus_Success) {
			data.device = device;
		}
		else {
			std::cerr << "Error while requesting device: " << toStdStringView(message) << std::endl;
		}
		data.requestEnded = true;
	};

	WGPURequestDeviceCallbackInfo callbackInfo = {};// see requestAdapterSync for `= {}`
	callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	callbackInfo.callback = onDeviceRequestEnded;
	callbackInfo.userdata1 = &userData;
	callbackInfo.userdata2 = nullptr;

	wgpuAdapterRequestDevice(adapter, descriptor, callbackInfo);

	waitUntilRequestEnded(instance, userData.requestEnded);

	return userData.device;
}
