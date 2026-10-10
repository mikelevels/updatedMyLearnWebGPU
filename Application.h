#pragma once
/**
 * Application.h: the app's state and the list of everything it does.
 *
 * REFACTOR (milestone): this used to be one 1000-line main.cpp. The class is
 * the same, but its work is now split into small functions (at most 25 lines
 * of code each), grouped by topic in several .cpp files:
 *
 *   Application.cpp           Initialize / Terminate / MainLoop: the big steps
 *   ApplicationDevice.cpp     window, instance, surface, adapter, device
 *   ApplicationPipeline.cpp   shader, vertex layout, blending, depth test, layouts
 *   ApplicationResources.cpp  buffers, uniforms, matrices, depth texture, bind group
 *   ApplicationFrame.cpp      everything that happens once per frame
 *   Transforms.h/.cpp         the hand-written matrices of Step054/Step055
 *
 * Reading order for a newcomer: main.cpp, then Application.cpp, then follow
 * the function calls into the other files.
 */
#include "webgpu/webgpu.hpp"

#include <GLFW/glfw3.h>

#include <array>// for std::array (Step043)
#include <cstddef>// for offsetof
#include <cstdint>
#include <vector>

#include "Transforms.h"// GLM + the hand-written matrices

/**
 * The C++ mirror of the `MyUniforms` struct declared in shader.wgsl.
 * The two MUST have the same memory layout, field for field, because we copy
 * this struct byte-for-byte into the uniform buffer.
 *
 * Memory layout rules (Step043, "More uniforms"), from the WGSL spec's
 * "address space layout constraints":
 *  1. ALIGNMENT: each field must start at an offset that is a multiple of its
 *     alignment. A mat4x4f and a vec4f have an alignment of 16 bytes, an f32
 *     of 4 bytes. So the big fields go first:
 *        offset   0: projectionMatrix (64 bytes)   <- Step055
 *        offset  64: viewMatrix       (64 bytes)   <- Step055
 *        offset 128: modelMatrix      (64 bytes)   <- Step055
 *        offset 192: color            (16 bytes)
 *        offset 208: time             ( 4 bytes)
 *        offset 212: gamma            ( 4 bytes)
 *        offset 216: _pad             ( 8 bytes)
 *  2. SIZE: the whole struct's size must be a multiple of its largest
 *     alignment (16 here), so we pad from 216 up to 224 bytes.
 * The WGSL side does not declare the padding: WGSL adds it implicitly.
 *
 * glm::mat4x4 stores its 16 floats column by column, exactly like WGSL's
 * mat4x4f, so it can be copied as is.
 */
struct MyUniforms {
	mat4x4 projectionMatrix; // camera space -> clip space (perspective)
	mat4x4 viewMatrix;       // world space -> camera space (where we look from)
	mat4x4 modelMatrix;      // model space -> world space (where the object is)
	std::array<float, 4> color; // RGBA tint applied to every fragment (vec4f in WGSL)
	float time;      // Seconds since the app started (glfwGetTime())
	float gamma;     // 2.2 when the surface is sRGB, 1.0 otherwise (see shader)
	float _pad[2];   // Unused, only here to reach 224 bytes (a multiple of 16)
};
// Have the compiler double check that we got the size right...
static_assert(sizeof(MyUniforms) % 16 == 0, "MyUniforms must be a multiple of 16 bytes");
// ...and that the fields really sit where WGSL expects them
static_assert(offsetof(MyUniforms, color) == 192, "color must start right after the 3 matrices");
static_assert(offsetof(MyUniforms, time) == 208, "time must start right after the 16-byte color");

/**
 * Main application class, that holds the whole app state and regroups
 * init/main loop/terminate functions.
 */
class Application{
    public:
        //Initialize everything return true on success
        bool Initialize();

        //Uninitialize everything
        void Terminate();

        // Draw a frame, handle events
        void MainLoop();

        // Return true as long as the main loop should keep running
        bool IsRunning();

    private:
		// --- ApplicationDevice.cpp: from a window to a working GPU device ---
		bool InitializeWindow();
		bool InitializeDevice();
		bool CreateInstanceAndSurface();
		wgpu::Adapter RequestAdapter();
		bool RequestDevice(wgpu::Adapter adapter);
		bool ConfigureSurface(wgpu::Adapter adapter);
		//[NEW API] RequiredLimits is gone, the required limits are a plain wgpu::Limits
		wgpu::Limits GetRequiredLimits(wgpu::Adapter adapter) const;

		// --- ApplicationPipeline.cpp: how to draw (returns false if the shader
		// could not be loaded) ---
		bool InitializePipeline();
		void CreateBindGroupLayout();
		void CreatePipelineLayout();
		wgpu::DepthStencilState MakeDepthStencilState() const;

		// --- ApplicationResources.cpp: what to draw ---
		//(returns false if the geometry file could not be loaded)
		bool InitializeBuffers();
		void CreateGeometryBuffers(const std::vector<float>& pointData, const std::vector<uint16_t>& indexData);
		void CreateUniformBuffer();
		void InitializeUniforms();
		void BuildMatricesByHand();
		void BuildMatricesWithGlm();
		void BuildMatricesWithGlmChain();
		void ChooseGamma();
		//Substep of Initialize() that creates the depth texture (Step052)
		void InitializeDepthBuffer();
		//Substep of Initialize() that connects the uniform buffer to the pipeline
		void InitializeBindGroups();

		// --- ApplicationFrame.cpp: one frame ---
		wgpu::TextureView GetNextSurfaceTextureView();
		void RecoverSurface(wgpu::SurfaceGetCurrentTextureStatus status);
		void UpdateUniforms();
		void RecordRenderPass(wgpu::CommandEncoder encoder, wgpu::TextureView targetView);
		wgpu::RenderPassColorAttachment MakeColorAttachment(wgpu::TextureView targetView) const;
		wgpu::RenderPassDepthStencilAttachment MakeDepthAttachment() const;
		void DrawScene(wgpu::RenderPassEncoder renderPass);
		void SubmitCommands(wgpu::CommandEncoder encoder);
		void PresentFrame(wgpu::TextureView targetView);

        // All the state shared by the methods of this class (private: only
        // the Application itself can touch it)
        GLFWwindow *window = nullptr;
		//[NEW API] The instance now lives until Terminate(). MainLoop() calls
		//instance.processEvents() every frame so that WebGPU can run our callbacks.
		//(In MyLearnWebGPU it was a local variable, released right after the adapter.)
		wgpu::Instance instance = nullptr;
        wgpu::Device device = nullptr;
        wgpu::Queue queue = nullptr;
        wgpu::Surface surface = nullptr;
		//[NEW API] No more std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallbackHandle:
		//the error callback is given in the DeviceDescriptor, there is no handle to keep alive.
		wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
		//[NEW API] The surface texture we draw into during the current frame. It is
		//released at the very END of MainLoop(), AFTER surface.present().
		wgpu::Texture currentSurfaceTexture = nullptr;
		//BUG FIX (bug review): the surface settings are kept, so the surface can
		//be configured again when it reports "Outdated" or "Lost" (for example
		//after the window was minimized and restored).
		wgpu::SurfaceConfiguration surfaceConfig = wgpu::Default;
		wgpu::RenderPipeline pipeline = nullptr;
		wgpu::Buffer pointBuffer = nullptr;
		wgpu::Buffer indexBuffer = nullptr;
		uint32_t indexCount = 0;

		// Uniforms (Step039): values that are the same for every vertex and
		// fragment of one draw call, but that we may change between frames.
		wgpu::Buffer uniformBuffer = nullptr;
		// The bind group LAYOUT says what kind of resources the pipeline expects
		// (here: one uniform buffer at @group(0) @binding(0))...
		wgpu::BindGroupLayout bindGroupLayout = nullptr;
		// ...the pipeline LAYOUT is the list of all bind group layouts...
		wgpu::PipelineLayout pipelineLayout = nullptr;
		// ...and the BIND GROUP is the actual buffer plugged into that slot.
		wgpu::BindGroup bindGroup = nullptr;

		// 2.2 if the surface does sRGB conversion, 1.0 otherwise (set in
		// ChooseGamma). Used for the shader AND for the clear color.
		float gamma = 1.0f;
		// Step050: the dynamic uniforms of Step044 (two logos from one buffer)
		// are rolled back, like in the guide, to keep the 3D code simple.

		// Depth buffer (Step052), a.k.a. Z-buffer: a texture the size of the
		// window that stores, for each pixel, the depth of the closest
		// fragment drawn so far. The pipeline needs its FORMAT, the render
		// pass needs a VIEW of it.
		wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
		wgpu::Texture depthTexture = nullptr;
		wgpu::TextureView depthTextureView = nullptr;

		// Step055: the CPU copy of the uniforms, kept so UpdateUniforms() can update
		// the model matrix each frame. The pieces of the model matrix that do
		// not change over time (scale and translation) are kept as well.
		MyUniforms uniforms = {};
		mat4x4 modelScale = mat4x4(1.0);       // S in the guide
		mat4x4 modelTranslation = mat4x4(1.0); // T1 in the guide
};
