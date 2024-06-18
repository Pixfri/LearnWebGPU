#include "Application.hpp"

#include "DeviceUtils.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <cstdint>
#include <iostream>
#include <vector>

const char *shaderSource = R"(
    @vertex
    fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
        var p = vec2f(0.0, 0.0);
        if (in_vertex_index == 0u) {
            p = vec2f(-0.5, -0.5);
        } else if (in_vertex_index == 1u) {
            p = vec2f(0.5, -0.5);
        } else {
            p = vec2f(0.0, 0.5);
        }
        return vec4f(p, 0.0, 1.0);
    }

    @fragment
    fn fs_main() -> @location(0) vec4f {
        return vec4f(0.0, 0.4, 1.0, 1.0);
    }
)";

bool Application::Initialize() {
    if (!glfwInit()) {
        std::cerr << "Could not intialize GLFW!" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_Window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

    if (!m_Window) {
        std::cerr << "Could not open window!";
        glfwTerminate();
        return false;
    }

    // We create a descriptor.
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;

    // We create the instance using this descriptor.
    WGPUInstance instance = wgpuCreateInstance(&desc);

    // We check whether there is actually an instance created.
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return false;
    }

    // Display the object (WGPUInstance is a simple pointer, it may be
    // copied around without worrying about its size).
    std::cout << "WGPU instance: " << instance << std::endl;

    std::cout << "Requesting adapter..." << std::endl;
    m_Surface = glfwGetWGPUSurface(instance, m_Window);

    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = m_Surface;

    WGPUAdapter adapter = RequestAdapterSync(instance, &adapterOpts);

    std::cout << "Got adapter: " << adapter << std::endl;

    // We display informations about the adapter.
    InspectAdapter(adapter);

    std::cout << "Requesting device..." << std::endl;

    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "WebGPU Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredFeatures = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const *message, void */* pUserData*/) {
        std::cout << "Device lost: reason " << reason;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    };

    m_Device = RequestDeviceSync(adapter, &deviceDesc);

    std::cout << "Got device: " << m_Device << std::endl;

    auto onDeviceError = [](WGPUErrorType type, char const *message, void */* pUserData */) {
        std::cout << "Uncaptured device error: type " << type;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(m_Device, onDeviceError, nullptr /* pUserData */),

    InspectDevice(m_Device);

    m_Queue = wgpuDeviceGetQueue(m_Device);

    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;

    config.width = 640;
    config.height = 480;

    m_SurfaceFormat = wgpuSurfaceGetPreferredFormat(m_Surface, adapter);
    config.format = m_SurfaceFormat;

    config.viewFormatCount = 0;
    config.viewFormats = nullptr;

    config.usage = WGPUTextureUsage_RenderAttachment;

    config.device = m_Device;

    config.presentMode = WGPUPresentMode_Fifo;

    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(m_Surface, &config);

    InitializePipeline();

    PlayingWithBuffers();

    return true;
}

void Application::Terminate(){
    wgpuBufferRelease(m_Buffer1);
    wgpuBufferRelease(m_Buffer2);
    wgpuRenderPipelineRelease(m_Pipeline);
    wgpuSurfaceUnconfigure(m_Surface);
    wgpuQueueRelease(m_Queue);
    wgpuSurfaceRelease(m_Surface);
    wgpuDeviceRelease(m_Device);
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Application::MainLoop() {
    glfwPollEvents();

    WGPUTextureView targetView = GetNextSurfaceTextureView();
    if (!targetView) return;

    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = "Command encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_Device,  &encoderDesc);

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;

    WGPURenderPassColorAttachment renderPassColorAttachment = {};

    renderPassColorAttachment.view = targetView;

    renderPassColorAttachment.resolveTarget = nullptr;

    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    // Select which render pipeline to use.
    wgpuRenderPassEncoderSetPipeline(renderPass, m_Pipeline);
    // Draw 1 instance of a 3-vertices triangle.
    wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    WGPUCommandBufferDescriptor commandBufferDesc = {};
    commandBufferDesc.nextInChain = nullptr;
    commandBufferDesc.label = "Command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(m_Queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuTextureViewRelease(targetView);
    wgpuSurfacePresent(m_Surface);

    wgpuDevicePoll(m_Device, false, nullptr);
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(m_Window);
}

WGPUTextureView Application::GetNextSurfaceTextureView() {
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(m_Surface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return nullptr;
    }

    WGPUTextureViewDescriptor viewDescriptor;
    viewDescriptor.nextInChain = nullptr;
    viewDescriptor.label= "Surface texture view";
    viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = WGPUTextureAspect_All;
    WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

    return targetView;
}

void Application::InitializePipeline() {
    WGPUShaderModuleDescriptor shaderDesc{};

    shaderDesc.hintCount = 0;
    shaderDesc.hints = nullptr;

    WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};
    // Set the chained struct's header
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    // Connect the chain.
    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    shaderCodeDesc.code = shaderSource;

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_Device, &shaderDesc);

    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.nextInChain = nullptr;

    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;

    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    // Each sequence of 3 vertices is considered a triangle.
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;

    // We'll see later how to specify the order in which vertices should be
    // connected. When not specified, vertices are considered sequencially.
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;

    // The face orientation is defined by assuming thate when looking
    // from the front of the face, its corner vertices are enumerated
    // in the counter-clockwise (CCW) order.
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;

    // But the face orientation does not matter much because we do not
    // cull (i.e. "hide") the faces pointing away from us (which is often
    // used for optimization).
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;

    // We tell that the programmable fragment shader stage is described
    // by the function called 'fs_main' in the shader module.
    WGPUFragmentState fragmentState{};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    WGPUBlendState blendState{};
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;

    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget{};
    colorTarget.format = m_SurfaceFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All; // We could write to only some of the color channels.

    // We have only one target because our render pass has only one output color
    // attachment.
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.fragment = &fragmentState;

    // We do not use stencil/depth testing for now.
    pipelineDesc.depthStencil = nullptr;

    // Samples per pixel
    pipelineDesc.multisample.count = 1;
    // Default value for the mask, meaning "all bits on".
    pipelineDesc.multisample.mask = ~0u;
    // Default value as well (irrelevant for count = 1 anyways).
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    pipelineDesc.layout = nullptr;

    m_Pipeline = wgpuDeviceCreateRenderPipeline(m_Device, &pipelineDesc);

    // We no longer need to access the shader module
    wgpuShaderModuleRelease(shaderModule);
}

void Application::PlayingWithBuffers() {
    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = "Some GPU-Side data buffer";
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    bufferDesc.size = 16;
    bufferDesc.mappedAtCreation = false;
    m_Buffer1 = wgpuDeviceCreateBuffer(m_Device, &bufferDesc);

    bufferDesc.label = "Output buffer";
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    m_Buffer2 = wgpuDeviceCreateBuffer(m_Device, &bufferDesc);

    // Create some CPU-side data buffer (of size 16 bytes).
    std::vector<uint8_t> numbers(16);
    for (uint8_t i = 0; i < 16; i++) numbers[i] = i;
    // `numbers` now contains  [ 0, 1, 2, ... ]

    // Copy this from `numbers` (RAM) to `buffer1` (VRAM).
    wgpuQueueWriteBuffer(m_Queue, m_Buffer1, 0, numbers.data(), numbers.size());

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_Device, nullptr);

    wgpuCommandEncoderCopyBufferToBuffer(encoder, m_Buffer1, 0, m_Buffer2, 0, 16);

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(m_Queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuDevicePoll(m_Device, false, nullptr);

    struct Context {
        bool Ready;
        WGPUBuffer Buffer;
    };

    auto onBuffer2Mapped = [](WGPUBufferMapAsyncStatus status, void *pUserData) {
        Context *context = static_cast<Context*>(pUserData);
        context->Ready = true;

        std::cout << "Buffer 2 mapped with status " << status << std::endl;
        if (status != WGPUBufferMapAsyncStatus_Success) return;

        auto bufferData = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(context->Buffer, 0, 16));

        std::cout << "BufferData = [";
        for (int i = 0; i < 16; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << static_cast<int>(bufferData[i]);
        }
        std::cout << "]" << std::endl;

        wgpuBufferUnmap(context->Buffer);
    };

    Context context = { false, m_Buffer2 };

    wgpuBufferMapAsync(m_Buffer2, WGPUMapMode_Read, 0, 16, onBuffer2Mapped, &context);

    while (!context.Ready) {
        wgpuDevicePoll(m_Device, false, nullptr);
    }
}
