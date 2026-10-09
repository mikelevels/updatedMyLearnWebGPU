#pragma once
//Small helpers for the NEW webgpu.h API, taken from the Next version of the guide
//(The Adapter / The Device chapters, files webgpu-utils.h/.cpp in branch step039-next):
//https://eliemichel.github.io/LearnWebGPU/next/getting-started/adapter-and-device/the-adapter.html
//They use the raw C types on purpose: a wgpu::Instance/wgpu::Adapter converts to
//WGPUInstance/WGPUAdapter by itself, and the WGPUAdapter/WGPUDevice we return
//converts back into a wgpu::Adapter/wgpu::Device.
#include <webgpu/webgpu.h>

#include <string_view>

/**
 * Turn a WGPUStringView that WebGPU gives us (callback messages, adapter info...)
 * into a std::string_view that std::cout can print.
 * Strings coming FROM WebGPU may or may not end with '\0', so never print
 * message.data directly.
 */
std::string_view toStdStringView(WGPUStringView wgpuStringView);

/**
 * Wait a little. On the Web this hands control back to the browser
 * (emscripten_sleep), which is the only way a request can ever finish there.
 */
void sleepForMilliseconds(unsigned int milliseconds);

/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapterSync(instance, options);
 * is roughly equivalent to the JavaScript
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options);

/**
 * Utility function to get a WebGPU device, so that
 *     WGPUDevice device = requestDeviceSync(instance, adapter, descriptor);
 * is roughly equivalent to
 *     const device = await adapter.requestDevice(descriptor);
 */
WGPUDevice requestDeviceSync(WGPUInstance instance, WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor);
