#include "Application.hpp"

#include "DeviceUtils.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <iostream>

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

    WGPUTextureFormat surfaceFormat = wgpuSurfaceGetPreferredFormat(m_Surface, adapter);
    config.format = surfaceFormat;

    config.viewFormatCount = 0;
    config.viewFormats = nullptr;

    config.usage = WGPUTextureUsage_RenderAttachment;

    config.device = m_Device;

    config.presentMode = WGPUPresentMode_Fifo;

    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(m_Surface, &config);

    return true;
}

void Application::Terminate(){
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
