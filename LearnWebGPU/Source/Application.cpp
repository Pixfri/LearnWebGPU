#include "Application.hpp"

#include "DeviceUtils.hpp"
#include "ModelLoader.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

const char *shaderSource = R"(
    /**
     * A structure with fields labeled with vertex attribute locations can be used
     * as input to the entry point of a shader.
     */
    struct VertexInput {
        @location(0) position: vec2f,
        @location(1) color: vec3f
    };

    /**
     * A structure with fields labeled with builtins and locations can also be used
     * as *output* of the vertex shader, which is also the input of the fragment
     * shader.
     */
    struct VertexOutput {
        @builtin(position) position: vec4f,
        // The location here does not refer to a vertex attribute, it just means
        // that this field must be handled by the rasterizer.
        // (It can also refer to another field of another struct that would be used
        // as input to the fragment shader.)
        @location(0) color: vec3f,
    };

    @vertex
    fn vs_main(in: VertexInput) -> VertexOutput {
        var out: VertexOutput; // Create the output struct.
        let ratio = 640.0 / 480.0; // The width and the height of the target surface.
        out.position = vec4f(in.position.x, in.position.y * ratio, 0.0, 1.0); // Same as what we used to directly return.
        out.color = in.color; // Forward the color attribute to the fragment shader.
        return out;
    }

    @fragment
    fn fs_main(in: VertexOutput) -> @location(0) vec4f {
        return vec4f(in.color, 1.0); // Use the interpolated color coming from the vertex shader.
    }
)";

bool Application::Initialize() {
    if (!glfwInit()) {
        std::cerr << "Could not intialize GLFW!\n";
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
    WGPUInstanceDescriptor desc;
    desc.nextInChain = nullptr;

    // We create the instance using this descriptor.
    WGPUInstance instance = wgpuCreateInstance(&desc);

    // We check whether there is actually an instance created.
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!\n";
        return false;
    }

    // Display the object (WGPUInstance is a simple pointer, it may be
    // copied around without worrying about its size).
    std::cout << "WGPU instance: " << instance << '\n';

    std::cout << "Requesting adapter...\n";
    m_Surface = glfwGetWGPUSurface(instance, m_Window);

    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = m_Surface;

    WGPUAdapter adapter = RequestAdapterSync(instance, &adapterOpts);

    std::cout << "Got adapter: " << adapter << '\n';

    // We display informations about the adapter.
    InspectAdapter(adapter);

    std::cout << "Requesting device..." << '\n';

    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "WebGPU Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredFeatures = nullptr;

    WGPURequiredLimits requiredLimits = GetRequiredLimits(adapter);
    deviceDesc.requiredLimits = &requiredLimits;
    
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const *message, void */* pUserData*/) {
        std::cout << "Device lost: reason " << reason;
        if (message) std::cout << " (" << message << ")";
        std::cout << '\n';
    };

    m_Device = RequestDeviceSync(adapter, &deviceDesc);

    std::cout << "Got device: " << m_Device << '\n';

    auto onDeviceError = [](WGPUErrorType type, char const *message, void */* pUserData */) {
        std::cout << "Uncaptured device error: type " << type;
        if (message) std::cout << " (" << message << ")";
        std::cout << '\n';
    };
    wgpuDeviceSetUncapturedErrorCallback(m_Device, onDeviceError, nullptr /* pUserData */),

    InspectDevice(m_Device);

    m_Queue = wgpuDeviceGetQueue(m_Device);

    WGPUSurfaceConfiguration config;
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

    InitializeBuffers();

    return true;
}

void Application::Terminate() const {
    wgpuBufferRelease(m_VertexBuffer);
    wgpuRenderPipelineRelease(m_Pipeline);
    wgpuSurfaceUnconfigure(m_Surface);
    wgpuQueueRelease(m_Queue);
    wgpuSurfaceRelease(m_Surface);
    wgpuDeviceRelease(m_Device);
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Application::MainLoop() const {
    glfwPollEvents();

    WGPUTextureView targetView = GetNextSurfaceTextureView();
    if (!targetView) return;

    WGPUCommandEncoderDescriptor encoderDesc;
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
    renderPassColorAttachment.clearValue = WGPUColor{ 0.05, 0.05, 0.05, 1.0 };

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    // Select which render pipeline to use.
    wgpuRenderPassEncoderSetPipeline(renderPass, m_Pipeline);

    // Set vertex buffer while encoding the render pass.
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_VertexBuffer, 0, wgpuBufferGetSize(m_VertexBuffer));
    // The second argument must correspond to the choice of uint16_t or uint32_t
    // we've done when creating the index buffer.
    wgpuRenderPassEncoderSetIndexBuffer(renderPass, m_IndexBuffer, WGPUIndexFormat_Uint16, 0, m_IndexCount * sizeof(uint16_t));
    
    wgpuRenderPassEncoderDrawIndexed(renderPass, m_IndexCount, 1, 0, 0, 0);

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    WGPUCommandBufferDescriptor commandBufferDesc;
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

bool Application::IsRunning() const {
    return !glfwWindowShouldClose(m_Window);
}

WGPUTextureView Application::GetNextSurfaceTextureView() const {
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

    WGPUShaderModuleWGSLDescriptor shaderCodeDesc;
    // Set the chained struct's header
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    // Connect the chain.
    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    shaderCodeDesc.code = shaderSource;

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_Device, &shaderDesc);

    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.nextInChain = nullptr;

    WGPUVertexBufferLayout vertexBufferLayout{};

    std::array<WGPUVertexAttribute, 2> vertexAttribs;

    // == For each attribute, describe its layout, i.e, how to interpret the raw data ==

    // Position
    // Corresponds to @location(...)
    vertexAttribs[0].shaderLocation = 0;
    // Means vec2f in the shader.
    vertexAttribs[0].format = WGPUVertexFormat_Float32x2;
    // Index of the first element.
    vertexAttribs[0].offset = 0;

    // Color
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = WGPUVertexFormat_Float32x3;
    vertexAttribs[1].offset = 2 * sizeof(float);

    vertexBufferLayout.attributeCount = vertexAttribs.size();
    vertexBufferLayout.attributes = vertexAttribs.data();

    // == Common to attributes from the same buffer ==
    vertexBufferLayout.arrayStride = 5 * sizeof(float);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

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

    WGPUBlendState blendState;
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

void Application::InitializeBuffers() {
    std::vector<float> pointData;
    std::vector<uint16_t> indexData;

    bool success = LoadGeometry("Resources/webgpu.txt", pointData, indexData);
    
    m_VertexCount = static_cast<uint32_t>(pointData.size() / 5);
    
    m_IndexCount = static_cast<int>(indexData.size());
    
    WGPUBufferDescriptor bufferDesc;
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = "Vertex buffer";
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    bufferDesc.size = pointData.size() * sizeof(float);
    bufferDesc.mappedAtCreation = false;
    m_VertexBuffer = wgpuDeviceCreateBuffer(m_Device, &bufferDesc);

    wgpuQueueWriteBuffer(m_Queue, m_VertexBuffer, 0, pointData.data(), bufferDesc.size);

    bufferDesc.label = "Index buffer";
    bufferDesc.size = indexData.size() * sizeof(uint16_t);
    bufferDesc.size = (bufferDesc.size + 3) & ~3; // Round up to the next multiple of 4.
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    m_IndexBuffer = wgpuDeviceCreateBuffer(m_Device, &bufferDesc);

    wgpuQueueWriteBuffer(m_Queue, m_IndexBuffer, 0, indexData.data(), bufferDesc.size);
}

// Initialize the WGPULimits structure.
void SetDefaults(WGPULimits &limits) {
    limits.maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindGroups = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindGroupsPlusVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindingsPerBindGroup = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxUniformBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
    limits.maxStorageBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
    limits.minUniformBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
    limits.minStorageBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBufferSize = WGPU_LIMIT_U64_UNDEFINED;
    limits.maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxColorAttachments = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxColorAttachmentBytesPerSample = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED;
}

WGPURequiredLimits Application::GetRequiredLimits(WGPUAdapter adapter) const {
    // Get the adapter supported limits, in case we need them.
    WGPUSupportedLimits supportedLimits;
    supportedLimits.nextInChain = nullptr;
    wgpuAdapterGetLimits(adapter, &supportedLimits);

    WGPURequiredLimits requiredLimits{};
    SetDefaults(requiredLimits.limits);

    // We use at most 2 vertex attribute for now.
    requiredLimits.limits.maxVertexAttributes = 2;
    // We should also tell that we use 1 vertex buffer.
    requiredLimits.limits.maxVertexBuffers = 1;
    // Maximum size of a buffer is 6 vertices of 5 floats each.
    requiredLimits.limits.maxBufferSize = 15 * 5 * sizeof(float);
    // Maximum stride between 5 consecutive vertices in the vertex buffer.
    requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);
    // There is a maximum of 3 floats forwarded from vertex to fragment shader.
    requiredLimits.limits.maxInterStageShaderComponents = 3;

    return requiredLimits;
}
