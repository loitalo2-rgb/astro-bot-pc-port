#pragma once

#include "../common/types.h"
#include "vulkan_types.h"
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace AstroPort {

struct GnfTexture;

struct MeshDrawRange {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

struct GpuDeviceInfo {
    std::string deviceName;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    size_t dedicatedVramMb = 0;
    bool isDiscreteGpu = false;
};

class VulkanBackend {
public:
    static VulkanBackend& Get();

    bool Initialize(void* windowHandle, uint32_t width, uint32_t height, bool vsync);
    void Shutdown();

    bool Resize(uint32_t newWidth, uint32_t newHeight);

    void BeginFrame();
    void EndFrame();

    // Atualiza a transformação do modelo Vulkan e seu deslocamento horizontal.
    void SetModelTransform(float rotY, float rotX, float zoom, float jump,
                           float horizontalOffset = 0.0f);

    // Envia uma textura GNF (mip 0) para a GPU e a liga ao shader de fragmento.
    bool UploadTexture(const GnfTexture& texture);

    bool IsViewportActive() const { return m_swapchainReady; }

    // Consultas de Hardware GPU
    const GpuDeviceInfo& GetSelectedGpu() const { return m_selectedGpu; }
    uint32_t GetCurrentWidth() const { return m_width; }
    uint32_t GetCurrentHeight() const { return m_height; }

    // Memoria AGC
    void* AllocateGarlicVram(size_t size);
    void* AllocateOnionShared(size_t size);

    // Criacao de Shaders Nativos na GPU a partir de SPIR-V
    uint64_t CreateShaderModuleFromSpirv(const std::vector<uint32_t>& spirvCode);
    void DestroyShaderModule(uint64_t shaderModuleHandle);

    bool IsVulkanReady() const { return m_vulkanReady; }

private:
    VulkanBackend() = default;

    bool m_initialized = false;
    bool m_vulkanReady = false;
    uint32_t m_width = 1920;
    uint32_t m_height = 1080;
    bool m_vsync = true;
    void* m_hwnd = nullptr;

    void* m_vkInstance = nullptr;
    void* m_vkPhysicalDevice = nullptr;
    void* m_vkDevice = nullptr;
    void* m_vkQueue = nullptr;
    uint32_t m_graphicsQueueFamily = 0;

    void* m_surface = nullptr;
    void* m_swapchain = nullptr;
    uint32_t m_swapchainFormat = 0;
    uint32_t m_swapchainImageCount = 0;
    uint32_t m_currentImageIndex = 0;
    bool m_frameAcquired = false;
    bool m_swapchainReady = false;
    std::vector<void*> m_swapchainImages;
    std::vector<void*> m_swapchainViews;
    std::vector<void*> m_framebuffers;

    void* m_depthImage = nullptr;
    void* m_depthMemory = nullptr;
    void* m_depthView = nullptr;

    void* m_renderPass = nullptr;
    void* m_pipelineLayout = nullptr;
    void* m_pipeline = nullptr;
    void* m_commandPool = nullptr;
    void* m_commandBuffer = nullptr;

    void* m_imageAvailable = nullptr;
    void* m_renderFinished = nullptr;
    void* m_fence = nullptr;

    void* m_vertexBuffer = nullptr;
    void* m_vertexMemory = nullptr;
    void* m_indexBuffer = nullptr;
    void* m_indexMemory = nullptr;
    uint32_t m_indexCount = 0;
    std::vector<MeshDrawRange> m_drawRanges;

    void* m_textureImage = nullptr;
    void* m_textureMemory = nullptr;
    void* m_textureView = nullptr;
    void* m_textureSampler = nullptr;

    // FSR 4 Compute Pipeline (RCAS)
    void* m_fsrComputePipeline = nullptr;
    void* m_fsrPipelineLayout = nullptr;
    bool  m_fsrInitialized = false;

    void* m_captureBuffer = nullptr;
    void* m_captureMemory = nullptr;
    bool m_captureRequested = false;
    bool m_captureDone = false;
    void* m_descriptorSetLayout = nullptr;
    void* m_descriptorPool = nullptr;
    void* m_descriptorSet = nullptr;
    bool m_textureReady = false;

    float m_meshCenter[3] = { 0.0f, 0.0f, 0.0f };
    float m_meshScale = 1.0f;
    float m_rotY = 0.0f;
    float m_rotX = 0.15f;
    float m_zoom = 1.0f;
    float m_jump = 0.0f;
    float m_horizontalOffset = 0.0f;

    GpuDeviceInfo m_selectedGpu;

    bool DetectGpuHardware();
    bool InitRealVulkan();
    bool CreateSwapchainResources();
    void DestroySwapchainResources();
    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      const void* data, void** outBuffer, void** outMemory);
    void DestroyBuffer(void* buffer, void* memory);
    bool CreateMeshBuffers();
    void DestroyMeshBuffers();
    bool CreateDescriptorResources();
    void DestroyDescriptorResources();
    bool CreatePipelineResources();
    void DestroyPipelineResources();
    void DestroyTextureResources();
};

} // namespace AstroPort
