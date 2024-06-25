#pragma once

#include <GLFW/glfw3.h>

#include <webgpu/webgpu.h>

class Application {

private:
    GLFWwindow *m_Window;
    WGPUDevice m_Device;
    WGPUQueue m_Queue;
    WGPUSurface m_Surface;
    WGPURenderPipeline m_Pipeline;
    WGPUTextureFormat m_SurfaceFormat = WGPUTextureFormat_Undefined;

    WGPUBuffer m_VertexBuffer;
    uint32_t m_VertexCount;
    WGPUBuffer m_IndexBuffer;
    uint32_t m_IndexCount;
    
public:
    bool Initialize();

    void Terminate() const;

    void MainLoop() const;

    bool IsRunning() const;

private:
    WGPUTextureView GetNextSurfaceTextureView() const;
    void InitializePipeline();
    void InitializeBuffers();
    WGPURequiredLimits GetRequiredLimits(WGPUAdapter adapter) const;
};
