#include "DeviceUtils.hpp"

#include <webgpu/webgpu.h>

#include <magic_enum/magic_enum.hpp>

#include <cassert>
#include <iostream>
#include <vector>

WGPUAdapter RequestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const *options) {
    // A simple structure holding the local information shared with the
    // onAdapterRequestEnded callback.
    struct UserData {
        WGPUAdapter Adapter = nullptr;
        bool RequestEnded = false;
    };
    UserData userData;

    // Callback called by wgpuInstanceRequestAdapter when the request returns.
    // This is a C++ lambda function, but could be any function defined in the
    // global scope. It must be non-capturing (the brackets [] are empty) so
    // that it behaves like a regular C function pointer, which is what
    // wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
    // is to convey what we want to capture through the pUserData pointer,
    // provided as the last argument of wgpuInstanceRequestAdapter and received
    // by the callback as its last argument.
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *pUserData) {
        auto &[Adapter, RequestEnded] = *static_cast<UserData *>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            Adapter = adapter;
        } else {
            std::cerr << "Could not get WebGPU adapter: " << message << '\n';
        }
        RequestEnded = true;
    };

    // Call to the WebGPU request adapter procedure.
    wgpuInstanceRequestAdapter(
        instance /* equivalent of navigator.gpu */,
        options,
        onAdapterRequestEnded,
        (void *) &userData
        );

    assert(userData.RequestEnded);

    return userData.Adapter;
}

WGPUDevice RequestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const *descriptor) {
    struct UserData {
        WGPUDevice Device = nullptr;
        bool RequestEnded = false;
    };
    UserData userData;

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *pUserData) {
        UserData &requestData = *static_cast<UserData *>(pUserData);
        if (status == WGPURequestDeviceStatus_Success) {
            requestData.Device = device;
        } else {
            std::cerr << "Could not get WebGPU device: " << message << '\n';
        }
        requestData.RequestEnded = true;
    };

    wgpuAdapterRequestDevice(
        adapter,
        descriptor,
        onDeviceRequestEnded,
        &userData
    );

    assert(userData.RequestEnded);

    return userData.Device;
}

void InspectAdapter(WGPUAdapter adapter) {
    // Limits
    WGPUSupportedLimits supportedLimits = {};
    supportedLimits.nextInChain = nullptr;
    if (wgpuAdapterGetLimits(adapter, &supportedLimits)) {
        std::cout << "Device limits:\n";
		std::cout << " - maxTextureDimension1D: " << supportedLimits.limits.maxTextureDimension1D << '\n';
		std::cout << " - maxTextureDimension2D: " << supportedLimits.limits.maxTextureDimension2D << '\n';
		std::cout << " - maxTextureDimension3D: " << supportedLimits.limits.maxTextureDimension3D << '\n';
		std::cout << " - maxTextureArrayLayers: " << supportedLimits.limits.maxTextureArrayLayers << '\n';
		std::cout << " - maxBindGroups: " << supportedLimits.limits.maxBindGroups << '\n';
		std::cout << " - maxDynamicUniformBuffersPerPipelineLayout: " << supportedLimits.limits.maxDynamicUniformBuffersPerPipelineLayout << '\n';
		std::cout << " - maxDynamicStorageBuffersPerPipelineLayout: " << supportedLimits.limits.maxDynamicStorageBuffersPerPipelineLayout << '\n';
		std::cout << " - maxSampledTexturesPerShaderStage: " << supportedLimits.limits.maxSampledTexturesPerShaderStage << '\n';
		std::cout << " - maxSamplersPerShaderStage: " << supportedLimits.limits.maxSamplersPerShaderStage << '\n';
		std::cout << " - maxStorageBuffersPerShaderStage: " << supportedLimits.limits.maxStorageBuffersPerShaderStage << '\n';
		std::cout << " - maxStorageTexturesPerShaderStage: " << supportedLimits.limits.maxStorageTexturesPerShaderStage << '\n';
		std::cout << " - maxUniformBuffersPerShaderStage: " << supportedLimits.limits.maxUniformBuffersPerShaderStage << '\n';
		std::cout << " - maxUniformBufferBindingSize: " << supportedLimits.limits.maxUniformBufferBindingSize << '\n';
		std::cout << " - maxStorageBufferBindingSize: " << supportedLimits.limits.maxStorageBufferBindingSize << '\n';
		std::cout << " - minUniformBufferOffsetAlignment: " << supportedLimits.limits.minUniformBufferOffsetAlignment << '\n';
		std::cout << " - minStorageBufferOffsetAlignment: " << supportedLimits.limits.minStorageBufferOffsetAlignment << '\n';
		std::cout << " - maxVertexBuffers: " << supportedLimits.limits.maxVertexBuffers << '\n';
		std::cout << " - maxVertexAttributes: " << supportedLimits.limits.maxVertexAttributes << '\n';
		std::cout << " - maxVertexBufferArrayStride: " << supportedLimits.limits.maxVertexBufferArrayStride << '\n';
		std::cout << " - maxInterStageShaderComponents: " << supportedLimits.limits.maxInterStageShaderComponents << '\n';
		std::cout << " - maxComputeWorkgroupStorageSize: " << supportedLimits.limits.maxComputeWorkgroupStorageSize << '\n';
		std::cout << " - maxComputeInvocationsPerWorkgroup: " << supportedLimits.limits.maxComputeInvocationsPerWorkgroup << '\n';
		std::cout << " - maxComputeWorkgroupSizeX: " << supportedLimits.limits.maxComputeWorkgroupSizeX << '\n';
		std::cout << " - maxComputeWorkgroupSizeY: " << supportedLimits.limits.maxComputeWorkgroupSizeY << '\n';
		std::cout << " - maxComputeWorkgroupSizeZ: " << supportedLimits.limits.maxComputeWorkgroupSizeZ << '\n';
		std::cout << " - maxComputeWorkgroupsPerDimension: " << supportedLimits.limits.maxComputeWorkgroupsPerDimension << '\n';
    }

    // Features
    std::vector<WGPUFeatureName> features;

    // Call the function a first time with a null return address, just to get
    // the entry count.
    size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);

    // Allocate memory (could be a new, or a malloc() if this were a C program).
    features.resize(featureCount);

    // Call the function a second time, with a non-null return address.
    wgpuAdapterEnumerateFeatures(adapter, features.data());

    std::cout << "Adapter features:\n";
	std::cout << std::hex;
    for (auto f : features) {
        std::cout << " - 0x" << f << '\n';
    }
	std::cout << std::dec;

    // Properties
    WGPUAdapterProperties properties = {};
    properties.nextInChain = nullptr;
    wgpuAdapterGetProperties(adapter, &properties);
    std::cout << "Adapter properties:\n";
    std::cout << " - vendorID: " << properties.vendorID << '\n';
    if (properties.vendorName) {
        std::cout << " - vendorName: " << properties.vendorName << '\n';
    }
    if (properties.architecture) {
        std::cout << " - architecture: " << properties.architecture << '\n';
    }
    std::cout << " - deviceID: " << properties.deviceID << '\n';
    if (properties.name) {
        std::cout << " - name: " << properties.name << '\n';
    }
    if (properties.driverDescription) {
        std::cout << " - driverDescription: " << properties.driverDescription << '\n';
    }

    std::cout << " - adapterType: " << magic_enum::enum_name<WGPUAdapterType>(properties.adapterType) << '\n';
    std::cout << " - backendType: " << magic_enum::enum_name<WGPUBackendType>(properties.backendType) << '\n';
}

void InspectDevice(WGPUDevice device) {
    // Limits
    WGPUSupportedLimits supportedLimits;
    supportedLimits.nextInChain = nullptr;
    if (wgpuDeviceGetLimits(device, &supportedLimits)) {
        std::cout << "Device limits:\n";
		std::cout << " - maxTextureDimension1D: " << supportedLimits.limits.maxTextureDimension1D << '\n';
		std::cout << " - maxTextureDimension2D: " << supportedLimits.limits.maxTextureDimension2D << '\n';
		std::cout << " - maxTextureDimension3D: " << supportedLimits.limits.maxTextureDimension3D << '\n';
		std::cout << " - maxTextureArrayLayers: " << supportedLimits.limits.maxTextureArrayLayers << '\n';
		std::cout << " - maxBindGroups: " << supportedLimits.limits.maxBindGroups << '\n';
		std::cout << " - maxDynamicUniformBuffersPerPipelineLayout: " << supportedLimits.limits.maxDynamicUniformBuffersPerPipelineLayout << '\n';
		std::cout << " - maxDynamicStorageBuffersPerPipelineLayout: " << supportedLimits.limits.maxDynamicStorageBuffersPerPipelineLayout << '\n';
		std::cout << " - maxSampledTexturesPerShaderStage: " << supportedLimits.limits.maxSampledTexturesPerShaderStage << '\n';
		std::cout << " - maxSamplersPerShaderStage: " << supportedLimits.limits.maxSamplersPerShaderStage << '\n';
		std::cout << " - maxStorageBuffersPerShaderStage: " << supportedLimits.limits.maxStorageBuffersPerShaderStage << '\n';
		std::cout << " - maxStorageTexturesPerShaderStage: " << supportedLimits.limits.maxStorageTexturesPerShaderStage << '\n';
		std::cout << " - maxUniformBuffersPerShaderStage: " << supportedLimits.limits.maxUniformBuffersPerShaderStage << '\n';
		std::cout << " - maxUniformBufferBindingSize: " << supportedLimits.limits.maxUniformBufferBindingSize << '\n';
		std::cout << " - maxStorageBufferBindingSize: " << supportedLimits.limits.maxStorageBufferBindingSize << '\n';
		std::cout << " - minUniformBufferOffsetAlignment: " << supportedLimits.limits.minUniformBufferOffsetAlignment << '\n';
		std::cout << " - minStorageBufferOffsetAlignment: " << supportedLimits.limits.minStorageBufferOffsetAlignment << '\n';
		std::cout << " - maxVertexBuffers: " << supportedLimits.limits.maxVertexBuffers << '\n';
		std::cout << " - maxVertexAttributes: " << supportedLimits.limits.maxVertexAttributes << '\n';
		std::cout << " - maxVertexBufferArrayStride: " << supportedLimits.limits.maxVertexBufferArrayStride << '\n';
		std::cout << " - maxInterStageShaderComponents: " << supportedLimits.limits.maxInterStageShaderComponents << '\n';
		std::cout << " - maxComputeWorkgroupStorageSize: " << supportedLimits.limits.maxComputeWorkgroupStorageSize << '\n';
		std::cout << " - maxComputeInvocationsPerWorkgroup: " << supportedLimits.limits.maxComputeInvocationsPerWorkgroup << '\n';
		std::cout << " - maxComputeWorkgroupSizeX: " << supportedLimits.limits.maxComputeWorkgroupSizeX << '\n';
		std::cout << " - maxComputeWorkgroupSizeY: " << supportedLimits.limits.maxComputeWorkgroupSizeY << '\n';
		std::cout << " - maxComputeWorkgroupSizeZ: " << supportedLimits.limits.maxComputeWorkgroupSizeZ << '\n';
		std::cout << " - maxComputeWorkgroupsPerDimension: " << supportedLimits.limits.maxComputeWorkgroupsPerDimension << '\n';
    }

    // Features
    std::vector<WGPUFeatureName> features;
    const size_t featureCount = wgpuDeviceEnumerateFeatures(device, nullptr);

    features.resize(featureCount);
    wgpuDeviceEnumerateFeatures(device, features.data());

    std::cout << "Device features:\n";
    for (auto f : features) {
    	std::cout << std::hex;
        std::cout << " - 0x" << f << '\n';
    	std::cout << std::dec;
    }
    std::cout << std::dec;
}