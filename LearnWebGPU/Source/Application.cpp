#include "Application.hpp"

#include "DeviceUtils.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif

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

#ifdef WEBGPU_BACKEND_DAWN
    WGPUDawnTogglesDescriptor toggles;
    toggles.chain.next = nullptr;
    toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
    toggles.disabledTogglesCount = 0;
    toggles.enabledTogglesCount = 1;
    const char *toggleName = "enable_immediate_error_handling";
    toggles.enabledToggles = &toggleName;

    desc.nextInChain = &toggles.chain
#endif

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

    return true;
}

void Application::Terminate() {
    wgpuQueueRelease(m_Queue);
    wgpuSurfaceRelease(m_Surface);
    wgpuDeviceRelease(m_Device);
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Application::MainLoop() {
    glfwPollEvents();

#if defined(WEBGPU_BACKEND_DAWN)
        wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(m_Device, false, nullptr);
#endif
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(m_Window);
}
