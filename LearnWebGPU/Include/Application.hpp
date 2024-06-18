#pragma once

#include <GLFW/glfw3.h>

#include <webgpu/webgpu.h>

class Application {
public:
    bool Initialize();

    void Terminate();

    void MainLoop();

    bool IsRunning();

private:
    GLFWwindow *m_Window;
    WGPUDevice m_Device;
    WGPUQueue m_Queue;
    WGPUSurface m_Surface;
    WGPURenderPipeline m_Pipeline;
    WGPUTextureFormat m_SurfaceFormat = WGPUTextureFormat_Undefined;

    WGPUTextureView GetNextSurfaceTextureView();
    void InitializePipeline();
};
