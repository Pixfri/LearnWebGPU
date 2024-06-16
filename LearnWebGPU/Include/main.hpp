#pragma once

#include <webgpu/webgpu.h>

/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapterSync(instance, options);
 * is roughly equivalent to
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter RequestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const *options);

void InspectAdapter(WGPUAdapter adapter);