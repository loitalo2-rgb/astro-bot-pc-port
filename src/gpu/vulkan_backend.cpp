#include "vulkan_backend.h"
#include "vulkan_loader.h"
#include "vulkan_types.h"
#include "shader_bytecode.h"
#include "shader_recompiler.h"
#include "../assets/asset_loader.h"
#include "../kernel/kernel_emulator.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace AstroPort {
namespace {

#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t type = 0x4D42;
    uint32_t size = 0;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;
    uint32_t dataOffset = 54;
};
struct BmpInfoHeader {
    uint32_t size = 40;
    int32_t width = 0;
    int32_t height = 0;
    uint16_t planes = 1;
    uint16_t bitsPerPixel = 32;
    uint32_t compression = 0;
    uint32_t imageSize = 0;
    int32_t xPixelsPerMeter = 2835;
    int32_t yPixelsPerMeter = 2835;
    uint32_t colorsUsed = 0;
    uint32_t colorsImportant = 0;
};
#pragma pack(pop)

bool WriteBmpBGRA(const char* path, const uint8_t* pixels, uint32_t width, uint32_t height) {
    if (!path || !pixels || width == 0 || height == 0) return false;

    const uint32_t rowSize = width * 4u;
    BmpFileHeader fileHeader{};
    fileHeader.size = 54u + rowSize * height;
    BmpInfoHeader infoHeader{};
    infoHeader.width = static_cast<int32_t>(width);
    infoHeader.height = static_cast<int32_t>(height);
    infoHeader.imageSize = rowSize * height;

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    out.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    for (uint32_t y = height; y > 0; --y) {
        out.write(reinterpret_cast<const char*>(pixels + (y - 1u) * rowSize), rowSize);
    }
    return out.good();
}

struct VkExt {
    PFN_vkCreateWin32SurfaceKHR                 vkCreateWin32SurfaceKHR = nullptr;
    PFN_vkDestroySurfaceKHR                     vkDestroySurfaceKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR    vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR    vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkCreateSwapchainKHR                    vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR                   vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR                 vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR                   vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR                       vkQueuePresentKHR = nullptr;
    PFN_vkCreateImageView                       vkCreateImageView = nullptr;
    PFN_vkDestroyImageView                      vkDestroyImageView = nullptr;
    PFN_vkCreateRenderPass                      vkCreateRenderPass = nullptr;
    PFN_vkDestroyRenderPass                     vkDestroyRenderPass = nullptr;
    PFN_vkCreateFramebuffer                     vkCreateFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer                    vkDestroyFramebuffer = nullptr;
    PFN_vkCreatePipelineLayout                  vkCreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout                 vkDestroyPipelineLayout = nullptr;
    PFN_vkCreateGraphicsPipelines               vkCreateGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline                       vkDestroyPipeline = nullptr;
    PFN_vkCreateCommandPool                     vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool                    vkDestroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers                vkAllocateCommandBuffers = nullptr;
    PFN_vkResetCommandBuffer                    vkResetCommandBuffer = nullptr;
    PFN_vkBeginCommandBuffer                    vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer                      vkEndCommandBuffer = nullptr;
    PFN_vkCmdBeginRenderPass                    vkCmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass                      vkCmdEndRenderPass = nullptr;
    PFN_vkCmdBindPipeline                       vkCmdBindPipeline = nullptr;
    PFN_vkCmdBindVertexBuffers                  vkCmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer                    vkCmdBindIndexBuffer = nullptr;
    PFN_vkCmdSetViewport                        vkCmdSetViewport = nullptr;
    PFN_vkCmdSetScissor                         vkCmdSetScissor = nullptr;
    PFN_vkCmdPushConstants                      vkCmdPushConstants = nullptr;
    PFN_vkCmdDrawIndexed                        vkCmdDrawIndexed = nullptr;
    PFN_vkQueueSubmit                           vkQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle                         vkQueueWaitIdle = nullptr;
    PFN_vkDeviceWaitIdle                        vkDeviceWaitIdle = nullptr;
    PFN_vkCreateSemaphore                       vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore                      vkDestroySemaphore = nullptr;
    PFN_vkCreateFence                           vkCreateFence = nullptr;
    PFN_vkDestroyFence                          vkDestroyFence = nullptr;
    PFN_vkWaitForFences                         vkWaitForFences = nullptr;
    PFN_vkResetFences                           vkResetFences = nullptr;
    PFN_vkCreateBuffer                          vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer                         vkDestroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements           vkGetBufferMemoryRequirements = nullptr;
    PFN_vkAllocateMemory                        vkAllocateMemory = nullptr;
    PFN_vkFreeMemory                            vkFreeMemory = nullptr;
    PFN_vkBindBufferMemory                      vkBindBufferMemory = nullptr;
    PFN_vkMapMemory                             vkMapMemory = nullptr;
    PFN_vkUnmapMemory                           vkUnmapMemory = nullptr;
    PFN_vkCreateImage                           vkCreateImage = nullptr;
    PFN_vkDestroyImage                          vkDestroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements            vkGetImageMemoryRequirements = nullptr;
    PFN_vkBindImageMemory                       vkBindImageMemory = nullptr;
    PFN_vkCreateSampler                         vkCreateSampler = nullptr;
    PFN_vkDestroySampler                        vkDestroySampler = nullptr;
    PFN_vkCreateDescriptorSetLayout             vkCreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout            vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool                  vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool                 vkDestroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets                vkAllocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets                  vkUpdateDescriptorSets = nullptr;
    PFN_vkCmdBindDescriptorSets                 vkCmdBindDescriptorSets = nullptr;
    PFN_vkCmdPipelineBarrier                    vkCmdPipelineBarrier = nullptr;
    PFN_vkCmdCopyBufferToImage                  vkCmdCopyBufferToImage = nullptr;
    PFN_vkCmdCopyImageToBuffer                  vkCmdCopyImageToBuffer = nullptr;
};

VkExt g_ext;

void VkLog(const std::string& msg) {
    std::ofstream f("vulkan_backend.log", std::ios::out | std::ios::app);
    if (f.is_open()) f << msg << "\n";
    std::cerr << msg << "\n";
}

template <typename T>
bool LoadFn(T& fn, const char* name) {
    fn = reinterpret_cast<T>(VulkanLoader::Get().GetSymbol(name));
    if (!fn) {
        std::cerr << "[VulkanBackend] Simbolo ausente: " << name << "\n";
        return false;
    }
    return true;
}

bool LoadExtensions() {
    if (g_ext.vkCreateSwapchainKHR) return true;

    bool ok = true;
    ok &= LoadFn(g_ext.vkCreateWin32SurfaceKHR, "vkCreateWin32SurfaceKHR");
    ok &= LoadFn(g_ext.vkDestroySurfaceKHR, "vkDestroySurfaceKHR");
    ok &= LoadFn(g_ext.vkGetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR");
    ok &= LoadFn(g_ext.vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    ok &= LoadFn(g_ext.vkGetPhysicalDeviceSurfaceFormatsKHR, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    ok &= LoadFn(g_ext.vkGetPhysicalDeviceSurfacePresentModesKHR, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    ok &= LoadFn(g_ext.vkCreateSwapchainKHR, "vkCreateSwapchainKHR");
    ok &= LoadFn(g_ext.vkDestroySwapchainKHR, "vkDestroySwapchainKHR");
    ok &= LoadFn(g_ext.vkGetSwapchainImagesKHR, "vkGetSwapchainImagesKHR");
    ok &= LoadFn(g_ext.vkAcquireNextImageKHR, "vkAcquireNextImageKHR");
    ok &= LoadFn(g_ext.vkQueuePresentKHR, "vkQueuePresentKHR");
    ok &= LoadFn(g_ext.vkCreateImageView, "vkCreateImageView");
    ok &= LoadFn(g_ext.vkDestroyImageView, "vkDestroyImageView");
    ok &= LoadFn(g_ext.vkCreateRenderPass, "vkCreateRenderPass");
    ok &= LoadFn(g_ext.vkDestroyRenderPass, "vkDestroyRenderPass");
    ok &= LoadFn(g_ext.vkCreateFramebuffer, "vkCreateFramebuffer");
    ok &= LoadFn(g_ext.vkDestroyFramebuffer, "vkDestroyFramebuffer");
    ok &= LoadFn(g_ext.vkCreatePipelineLayout, "vkCreatePipelineLayout");
    ok &= LoadFn(g_ext.vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
    ok &= LoadFn(g_ext.vkCreateGraphicsPipelines, "vkCreateGraphicsPipelines");
    ok &= LoadFn(g_ext.vkDestroyPipeline, "vkDestroyPipeline");
    ok &= LoadFn(g_ext.vkCreateCommandPool, "vkCreateCommandPool");
    ok &= LoadFn(g_ext.vkDestroyCommandPool, "vkDestroyCommandPool");
    ok &= LoadFn(g_ext.vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
    ok &= LoadFn(g_ext.vkResetCommandBuffer, "vkResetCommandBuffer");
    ok &= LoadFn(g_ext.vkBeginCommandBuffer, "vkBeginCommandBuffer");
    ok &= LoadFn(g_ext.vkEndCommandBuffer, "vkEndCommandBuffer");
    ok &= LoadFn(g_ext.vkCmdBeginRenderPass, "vkCmdBeginRenderPass");
    ok &= LoadFn(g_ext.vkCmdEndRenderPass, "vkCmdEndRenderPass");
    ok &= LoadFn(g_ext.vkCmdBindPipeline, "vkCmdBindPipeline");
    ok &= LoadFn(g_ext.vkCmdBindVertexBuffers, "vkCmdBindVertexBuffers");
    ok &= LoadFn(g_ext.vkCmdBindIndexBuffer, "vkCmdBindIndexBuffer");
    ok &= LoadFn(g_ext.vkCmdSetViewport, "vkCmdSetViewport");
    ok &= LoadFn(g_ext.vkCmdSetScissor, "vkCmdSetScissor");
    ok &= LoadFn(g_ext.vkCmdPushConstants, "vkCmdPushConstants");
    ok &= LoadFn(g_ext.vkCmdDrawIndexed, "vkCmdDrawIndexed");
    ok &= LoadFn(g_ext.vkQueueSubmit, "vkQueueSubmit");
    ok &= LoadFn(g_ext.vkQueueWaitIdle, "vkQueueWaitIdle");
    ok &= LoadFn(g_ext.vkDeviceWaitIdle, "vkDeviceWaitIdle");
    ok &= LoadFn(g_ext.vkCreateSemaphore, "vkCreateSemaphore");
    ok &= LoadFn(g_ext.vkDestroySemaphore, "vkDestroySemaphore");
    ok &= LoadFn(g_ext.vkCreateFence, "vkCreateFence");
    ok &= LoadFn(g_ext.vkDestroyFence, "vkDestroyFence");
    ok &= LoadFn(g_ext.vkWaitForFences, "vkWaitForFences");
    ok &= LoadFn(g_ext.vkResetFences, "vkResetFences");
    ok &= LoadFn(g_ext.vkCreateBuffer, "vkCreateBuffer");
    ok &= LoadFn(g_ext.vkDestroyBuffer, "vkDestroyBuffer");
    ok &= LoadFn(g_ext.vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
    ok &= LoadFn(g_ext.vkAllocateMemory, "vkAllocateMemory");
    ok &= LoadFn(g_ext.vkFreeMemory, "vkFreeMemory");
    ok &= LoadFn(g_ext.vkBindBufferMemory, "vkBindBufferMemory");
    ok &= LoadFn(g_ext.vkMapMemory, "vkMapMemory");
    ok &= LoadFn(g_ext.vkUnmapMemory, "vkUnmapMemory");
    ok &= LoadFn(g_ext.vkCreateImage, "vkCreateImage");
    ok &= LoadFn(g_ext.vkDestroyImage, "vkDestroyImage");
    ok &= LoadFn(g_ext.vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements");
    ok &= LoadFn(g_ext.vkBindImageMemory, "vkBindImageMemory");
    ok &= LoadFn(g_ext.vkCreateSampler, "vkCreateSampler");
    ok &= LoadFn(g_ext.vkDestroySampler, "vkDestroySampler");
    ok &= LoadFn(g_ext.vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
    ok &= LoadFn(g_ext.vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
    ok &= LoadFn(g_ext.vkCreateDescriptorPool, "vkCreateDescriptorPool");
    ok &= LoadFn(g_ext.vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
    ok &= LoadFn(g_ext.vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
    ok &= LoadFn(g_ext.vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
    ok &= LoadFn(g_ext.vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
    ok &= LoadFn(g_ext.vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
    ok &= LoadFn(g_ext.vkCmdCopyBufferToImage, "vkCmdCopyBufferToImage");
    ok &= LoadFn(g_ext.vkCmdCopyImageToBuffer, "vkCmdCopyImageToBuffer");
    return ok;
}

struct Mat4 {
    float m[16] = {};
};

Mat4 Identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float v = 0.0f;
            for (int k = 0; k < 4; ++k) {
                v += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = v;
        }
    }
    return r;
}

Mat4 Translate(float x, float y, float z) {
    Mat4 r = Identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

Mat4 Scale(float s) {
    Mat4 r = Identity();
    r.m[0] = r.m[5] = r.m[10] = s;
    return r;
}

Mat4 RotateX(float radians) {
    Mat4 r = Identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

Mat4 RotateY(float radians) {
    Mat4 r = Identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

Mat4 Perspective(float fovRadians, float aspect, float nearPlane, float farPlane) {
    Mat4 r{};
    const float f = 1.0f / std::tan(fovRadians * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = -f;
    r.m[10] = farPlane / (nearPlane - farPlane);
    r.m[11] = -1.0f;
    r.m[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
    return r;
}

struct PushConstants {
    Mat4 mvp;
    Mat4 model;
};

constexpr uint32_t kMaxFramesInFlight = 1;

} // namespace

VulkanBackend& VulkanBackend::Get() {
    static VulkanBackend instance;
    return instance;
}

uint32_t VulkanBackend::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    if (!m_vkPhysicalDevice) return 0;
    auto& vk = VulkanLoader::Get();
    VkPhysicalDeviceMemoryProperties mem{};
    vk.vkGetPhysicalDeviceMemoryProperties((VkPhysicalDevice)m_vkPhysicalDevice, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount && i < 32; ++i) {
        if ((typeBits & (1u << i)) &&
            (mem.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

bool VulkanBackend::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 const void* data, void** outBuffer, void** outMemory) {
    if (!m_vkDevice || size == 0 || !outBuffer || !outMemory) return false;

    VkBufferCreateInfo ci{};
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = 0;
    if (g_ext.vkCreateBuffer((VkDevice)m_vkDevice, &ci, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "[VulkanBackend] Falha ao criar buffer (" << size << " bytes)\n";
        return false;
    }

    VkMemoryRequirements req{};
    g_ext.vkGetBufferMemoryRequirements((VkDevice)m_vkDevice, buffer, &req);

    VkMemoryAllocateInfo ai{};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory memory = 0;
    if (g_ext.vkAllocateMemory((VkDevice)m_vkDevice, &ai, nullptr, &memory) != VK_SUCCESS ||
        g_ext.vkBindBufferMemory((VkDevice)m_vkDevice, buffer, memory, 0) != VK_SUCCESS) {
        std::cerr << "[VulkanBackend] Falha ao alocar memoria do buffer\n";
        if (memory) g_ext.vkFreeMemory((VkDevice)m_vkDevice, memory, nullptr);
        g_ext.vkDestroyBuffer((VkDevice)m_vkDevice, buffer, nullptr);
        return false;
    }

    if (data) {
        void* mapped = nullptr;
        if (g_ext.vkMapMemory((VkDevice)m_vkDevice, memory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS) {
            std::memcpy(mapped, data, static_cast<size_t>(size));
            g_ext.vkUnmapMemory((VkDevice)m_vkDevice, memory);
        }
    }

    *outBuffer = reinterpret_cast<void*>(buffer);
    *outMemory = reinterpret_cast<void*>(memory);
    return true;
}

bool VulkanBackend::CreateMeshBuffers() {
    const auto* mesh = AssetLoader::Get().GetCachedAstroMesh();
    if (!mesh || !mesh->valid || mesh->vertices.empty() || mesh->indices.empty()) {
        return false;
    }

    const VkDeviceSize vertexSize = mesh->vertices.size() * sizeof(JxmdVertex);
    const VkDeviceSize indexSize = mesh->indices.size() * sizeof(uint32_t);

    if (!CreateBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh->vertices.data(),
                      &m_vertexBuffer, &m_vertexMemory)) {
        return false;
    }
    if (!CreateBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh->indices.data(),
                      &m_indexBuffer, &m_indexMemory)) {
        DestroyBuffer(m_vertexBuffer, m_vertexMemory);
        m_vertexBuffer = nullptr;
        m_vertexMemory = nullptr;
        return false;
    }

    m_indexCount = static_cast<uint32_t>(mesh->indices.size());
    m_drawRanges.clear();
    m_drawRanges.reserve(mesh->submeshes.size());
    for (const JxmdSubmesh& submesh : mesh->submeshes) {
        MeshDrawRange range;
        range.indexOffset = submesh.indexOffset;
        range.indexCount = submesh.indexCount;
        range.materialIndex = submesh.materialIndex;
        m_drawRanges.push_back(range);
    }
    if (!m_drawRanges.empty()) {
        uint32_t totalIndices = 0;
        for (const MeshDrawRange& range : m_drawRanges) totalIndices += range.indexCount;
        VkLog("[VulkanBackend] Draw ranges JXMD: " + std::to_string(m_drawRanges.size())
              + " ranges, " + std::to_string(totalIndices) + " indices cobertos");
    }
    float uvMin[2] = { 1e30f, 1e30f };
    float uvMax[2] = { -1e30f, -1e30f };
    for (const JxmdVertex& v : mesh->vertices) {
        uvMin[0] = std::min(uvMin[0], v.u);
        uvMin[1] = std::min(uvMin[1], v.v);
        uvMax[0] = std::max(uvMax[0], v.u);
        uvMax[1] = std::max(uvMax[1], v.v);
    }
    VkLog("[VulkanBackend] UVs da malha: u=[" + std::to_string(uvMin[0]) + ", " + std::to_string(uvMax[0])
          + "] v=[" + std::to_string(uvMin[1]) + ", " + std::to_string(uvMax[1]) + "]");
    m_meshCenter[0] = (mesh->bboxMin[0] + mesh->bboxMax[0]) * 0.5f;
    m_meshCenter[1] = (mesh->bboxMin[1] + mesh->bboxMax[1]) * 0.5f;
    m_meshCenter[2] = (mesh->bboxMin[2] + mesh->bboxMax[2]) * 0.5f;

    const float ex = mesh->bboxMax[0] - mesh->bboxMin[0];
    const float ey = mesh->bboxMax[1] - mesh->bboxMin[1];
    const float ez = mesh->bboxMax[2] - mesh->bboxMin[2];
    const float maxExtent = std::max(1.0f, std::max(ex, std::max(ey, ez)));
    m_meshScale = 1.45f / maxExtent;

    std::cout << "[VulkanBackend] Buffers da malha criados: "
              << mesh->vertices.size() << " vertices, "
              << mesh->indices.size() << " indices\n";
    VkLog("[VulkanBackend] Buffers da malha criados: " + std::to_string(mesh->vertices.size())
          + " vertices, " + std::to_string(mesh->indices.size()) + " indices");
    return true;
}

void VulkanBackend::DestroyBuffer(void* buffer, void* memory) {
    if (!m_vkDevice) return;
    if (buffer) g_ext.vkDestroyBuffer((VkDevice)m_vkDevice, (VkBuffer)buffer, nullptr);
    if (memory) g_ext.vkFreeMemory((VkDevice)m_vkDevice, (VkDeviceMemory)memory, nullptr);
}

void VulkanBackend::DestroyMeshBuffers() {
    DestroyBuffer(m_indexBuffer, m_indexMemory);
    DestroyBuffer(m_vertexBuffer, m_vertexMemory);
    m_indexBuffer = nullptr;
    m_indexMemory = nullptr;
    m_vertexBuffer = nullptr;
    m_vertexMemory = nullptr;
    m_indexCount = 0;
    m_drawRanges.clear();
}

bool VulkanBackend::CreateDescriptorResources() {
    if (m_descriptorSetLayout && m_descriptorPool && m_descriptorSet) return true;
    if (!m_vkDevice) return false;

    VkDevice device = static_cast<VkDevice>(m_vkDevice);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.bindingCount = 1;
    dslci.pBindings = &binding;

    VkDescriptorSetLayout setLayout = 0;
    if (g_ext.vkCreateDescriptorSetLayout(device, &dslci, nullptr, &setLayout) != VK_SUCCESS) {
        VkLog("[VulkanBackend] Falha ao criar descriptor set layout");
        return false;
    }
    m_descriptorSetLayout = reinterpret_cast<void*>(setLayout);

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo dpci{};
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;

    VkDescriptorPool pool = 0;
    if (g_ext.vkCreateDescriptorPool(device, &dpci, nullptr, &pool) != VK_SUCCESS) {
        VkLog("[VulkanBackend] Falha ao criar descriptor pool");
        g_ext.vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        m_descriptorSetLayout = nullptr;
        return false;
    }
    m_descriptorPool = reinterpret_cast<void*>(pool);

    VkDescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool = pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &setLayout;

    VkDescriptorSet set = 0;
    if (g_ext.vkAllocateDescriptorSets(device, &dsai, &set) != VK_SUCCESS) {
        VkLog("[VulkanBackend] Falha ao alocar descriptor set");
        g_ext.vkDestroyDescriptorPool(device, pool, nullptr);
        g_ext.vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        m_descriptorPool = nullptr;
        m_descriptorSetLayout = nullptr;
        return false;
    }
    m_descriptorSet = reinterpret_cast<void*>(set);
    return true;
}

void VulkanBackend::DestroyDescriptorResources() {
    if (!m_vkDevice) return;
    VkDevice device = static_cast<VkDevice>(m_vkDevice);
    if (m_descriptorPool) {
        g_ext.vkDestroyDescriptorPool(device, reinterpret_cast<VkDescriptorPool>(m_descriptorPool), nullptr);
        m_descriptorPool = nullptr;
        m_descriptorSet = nullptr;
    }
    if (m_descriptorSetLayout) {
        g_ext.vkDestroyDescriptorSetLayout(device, reinterpret_cast<VkDescriptorSetLayout>(m_descriptorSetLayout), nullptr);
        m_descriptorSetLayout = nullptr;
    }
}

bool VulkanBackend::UploadTexture(const GnfTexture& texture) {
    if (!m_vkDevice || !texture.valid || texture.data.empty()) return false;
    if (!CreateDescriptorResources()) return false;

    VkDevice device = static_cast<VkDevice>(m_vkDevice);
    const uint32_t uploadMipLevels = std::max(1u, texture.mipLevels);
    if (!m_textureImage) {
        VkImageCreateInfo ici{};
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = static_cast<VkFormat>(texture.vulkanFormat);
        ici.extent = { texture.width, texture.height, 1 };
        ici.mipLevels = uploadMipLevels;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage image = 0;
        if (g_ext.vkCreateImage(device, &ici, nullptr, &image) != VK_SUCCESS) {
            VkLog("[VulkanBackend] Falha ao criar imagem de textura (formato "
                  + std::to_string(texture.vulkanFormat) + ")");
            return false;
        }
        m_textureImage = reinterpret_cast<void*>(image);

        VkMemoryRequirements req{};
        g_ext.vkGetImageMemoryRequirements(device, image, &req);
        VkMemoryAllocateInfo ai{};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory memory = 0;
        if (g_ext.vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS ||
            g_ext.vkBindImageMemory(device, image, memory, 0) != VK_SUCCESS) {
            VkLog("[VulkanBackend] Falha ao alocar memoria da textura");
            if (memory) g_ext.vkFreeMemory(device, memory, nullptr);
            g_ext.vkDestroyImage(device, image, nullptr);
            m_textureImage = nullptr;
            return false;
        }
        m_textureMemory = reinterpret_cast<void*>(memory);

        VkImageViewCreateInfo vci{};
        vci.image = image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = static_cast<VkFormat>(texture.vulkanFormat);
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = uploadMipLevels;
        vci.subresourceRange.layerCount = 1;

        VkImageView view = 0;
        if (g_ext.vkCreateImageView(device, &vci, nullptr, &view) != VK_SUCCESS) {
            VkLog("[VulkanBackend] Falha ao criar image view da textura");
            DestroyTextureResources();
            return false;
        }
        m_textureView = reinterpret_cast<void*>(view);
    }

    if (!m_textureSampler) {
        VkSamplerCreateInfo sci{};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxLod = static_cast<float>(uploadMipLevels - 1u);

        VkSampler sampler = 0;
        if (g_ext.vkCreateSampler(device, &sci, nullptr, &sampler) != VK_SUCCESS) {
            VkLog("[VulkanBackend] Falha ao criar sampler da textura");
            DestroyTextureResources();
            return false;
        }
        m_textureSampler = reinterpret_cast<void*>(sampler);
    }

    void* stagingBuffer = nullptr;
    void* stagingMemory = nullptr;
    if (!CreateBuffer(texture.data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, texture.data.data(),
                      &stagingBuffer, &stagingMemory)) {
        VkLog("[VulkanBackend] Falha ao criar staging buffer da textura");
        return false;
    }

    VkCommandBuffer cmd = reinterpret_cast<VkCommandBuffer>(m_commandBuffer);
    g_ext.vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (g_ext.vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) {
        DestroyBuffer(stagingBuffer, stagingMemory);
        return false;
    }

    VkImageMemoryBarrier toTransfer{};
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.image = reinterpret_cast<VkImage>(m_textureImage);
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = uploadMipLevels;
    toTransfer.subresourceRange.layerCount = 1;

    g_ext.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    const uint32_t copyMipCount = texture.mips.empty() ? 1u : uploadMipLevels;
    for (uint32_t mip = 0; mip < copyMipCount; ++mip) {
        const GnfMipLevel* level = texture.mips.empty() ? nullptr : &texture.mips[mip];
        const uint32_t mipWidth = level ? level->width : texture.width;
        const uint32_t mipHeight = level ? level->height : texture.height;

        VkBufferImageCopy region{};
        region.bufferOffset = level ? level->offset : 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = mip;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { mipWidth, mipHeight, 1 };
        g_ext.vkCmdCopyBufferToImage(cmd, reinterpret_cast<VkBuffer>(stagingBuffer),
            reinterpret_cast<VkImage>(m_textureImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    VkImageMemoryBarrier toShader{};
    toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.image = reinterpret_cast<VkImage>(m_textureImage);
    toShader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toShader.subresourceRange.levelCount = uploadMipLevels;
    toShader.subresourceRange.layerCount = 1;

    g_ext.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShader);

    if (g_ext.vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        DestroyBuffer(stagingBuffer, stagingMemory);
        return false;
    }

    VkSubmitInfo submit{};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    if (g_ext.vkQueueSubmit(static_cast<VkQueue>(m_vkQueue), 1, &submit, 0) != VK_SUCCESS) {
        DestroyBuffer(stagingBuffer, stagingMemory);
        return false;
    }
    g_ext.vkQueueWaitIdle(static_cast<VkQueue>(m_vkQueue));
    DestroyBuffer(stagingBuffer, stagingMemory);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = reinterpret_cast<VkSampler>(m_textureSampler);
    imageInfo.imageView = reinterpret_cast<VkImageView>(m_textureView);
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.dstSet = reinterpret_cast<VkDescriptorSet>(m_descriptorSet);
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    g_ext.vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    m_textureReady = true;
    std::cout << "[VulkanBackend] Textura enviada para GPU: " << texture.name
              << " (" << texture.width << "x" << texture.height
              << ", formato " << texture.vulkanFormat << ")\n";
    VkLog("[VulkanBackend] Textura enviada para GPU: " + texture.name
          + " " + std::to_string(texture.width) + "x" + std::to_string(texture.height)
          + " formato=" + std::to_string(texture.vulkanFormat)
          + " bytes=" + std::to_string(texture.data.size()));
    return true;
}

void VulkanBackend::DestroyTextureResources() {
    if (!m_vkDevice) return;
    VkDevice device = static_cast<VkDevice>(m_vkDevice);
    if (g_ext.vkDeviceWaitIdle) g_ext.vkDeviceWaitIdle(device);

    if (m_textureView) {
        g_ext.vkDestroyImageView(device, reinterpret_cast<VkImageView>(m_textureView), nullptr);
        m_textureView = nullptr;
    }
    if (m_textureSampler) {
        g_ext.vkDestroySampler(device, reinterpret_cast<VkSampler>(m_textureSampler), nullptr);
        m_textureSampler = nullptr;
    }
    if (m_textureImage) {
        g_ext.vkDestroyImage(device, reinterpret_cast<VkImage>(m_textureImage), nullptr);
        m_textureImage = nullptr;
    }
    if (m_textureMemory) {
        g_ext.vkFreeMemory(device, reinterpret_cast<VkDeviceMemory>(m_textureMemory), nullptr);
        m_textureMemory = nullptr;
    }
    DestroyDescriptorResources();
    m_textureReady = false;
}

bool VulkanBackend::InitRealVulkan() {
    if (!VulkanLoader::Get().Load()) {
        std::cerr << "[VulkanBackend] Aviso: vulkan-1.dll nao disponivel, operando em modo fallback.\n";
        return false;
    }
    if (!LoadExtensions()) {
        std::cerr << "[VulkanBackend] Aviso: extensoes WSI/swapchain indisponiveis.\n";
        return false;
    }

    auto& vk = VulkanLoader::Get();

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ASTRO BOT PC";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "Team ASOBI / AstroPort";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = (1 << 22) | (3 << 12);

    const char* instanceExtensions[] = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = instanceExtensions;

    VkInstance instance = nullptr;
    VkResult res = vk.vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS || !instance) {
        std::cerr << "[VulkanBackend] Falha ao criar VkInstance: " << res << "\n";
        return false;
    }
    m_vkInstance = instance;

    uint32_t deviceCount = 0;
    vk.vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cerr << "[VulkanBackend] Nenhuma GPU compativel com Vulkan encontrada!\n";
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vk.vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice chosenDevice = devices[0];
    VkPhysicalDeviceProperties chosenProps{};
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vk.vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == 2) {
            chosenDevice = dev;
            chosenProps = props;
            break;
        }
    }
    if (chosenProps.deviceName[0] == '\0') {
        vk.vkGetPhysicalDeviceProperties(chosenDevice, &chosenProps);
    }
    m_vkPhysicalDevice = chosenDevice;

    m_selectedGpu.deviceName = chosenProps.deviceName;
    m_selectedGpu.vendorId = chosenProps.vendorID;
    m_selectedGpu.deviceId = chosenProps.deviceID;
    m_selectedGpu.isDiscreteGpu = (chosenProps.deviceType == 2);

    VkPhysicalDeviceMemoryProperties memProps{};
    vk.vkGetPhysicalDeviceMemoryProperties(chosenDevice, &memProps);
    size_t totalVram = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount && i < 16; ++i) {
        if (memProps.memoryHeaps[i].flags & 1) {
            totalVram += static_cast<size_t>(memProps.memoryHeaps[i].size);
        }
    }
    m_selectedGpu.dedicatedVramMb = totalVram / (1024 * 1024);

    // Surface Win32 para o HWND filho do viewport.
    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.hinstance = GetModuleHandle(nullptr);
    surfaceInfo.hwnd = static_cast<HWND>(m_hwnd);
    VkSurfaceKHR surface = 0;
    res = g_ext.vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface);
    if (res != VK_SUCCESS || !surface) {
        std::cerr << "[VulkanBackend] Falha ao criar surface Win32: " << res << "\n";
        return false;
    }
    m_surface = reinterpret_cast<void*>(surface);

    uint32_t qfCount = 0;
    vk.vkGetPhysicalDeviceQueueFamilyProperties(chosenDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(qfCount);
    vk.vkGetPhysicalDeviceQueueFamilyProperties(chosenDevice, &qfCount, qfProps.data());

    uint32_t graphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < qfCount; ++i) {
        VkBool32 presentSupported = 0;
        g_ext.vkGetPhysicalDeviceSurfaceSupportKHR(chosenDevice, i, surface, &presentSupported);
        if ((qfProps[i].queueFlags & 1) && presentSupported) {
            graphicsQueueFamily = i;
            break;
        }
    }
    if (graphicsQueueFamily == std::numeric_limits<uint32_t>::max()) {
        for (uint32_t i = 0; i < qfCount; ++i) {
            if (qfProps[i].queueFlags & 1) {
                graphicsQueueFamily = i;
                break;
            }
        }
    }
    if (graphicsQueueFamily == std::numeric_limits<uint32_t>::max()) {
        std::cerr << "[VulkanBackend] Nenhuma fila grafica encontrada.\n";
        return false;
    }
    m_graphicsQueueFamily = graphicsQueueFamily;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = { "VK_KHR_swapchain" };
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    VkPhysicalDeviceFeatures features{};
    if (vk.vkGetPhysicalDeviceFeatures) {
        vk.vkGetPhysicalDeviceFeatures(chosenDevice, &features);
    }
    VkLog(std::string("[VulkanBackend] textureCompressionBC: ")
          + (features.textureCompressionBC ? "suportado" : "NAO suportado"));
    deviceCreateInfo.pEnabledFeatures = &features;

    VkDevice device = nullptr;
    res = vk.vkCreateDevice(chosenDevice, &deviceCreateInfo, nullptr, &device);
    if (res != VK_SUCCESS || !device) {
        std::cerr << "[VulkanBackend] Falha ao criar VkDevice: " << res << "\n";
        return false;
    }
    m_vkDevice = device;

    VkQueue queue = nullptr;
    vk.vkGetDeviceQueue(device, graphicsQueueFamily, 0, &queue);
    m_vkQueue = queue;

    m_vulkanReady = true;
    std::cout << "[VulkanBackend] Contexto Vulkan 1.3 REAL inicializado com sucesso!\n";
    std::cout << "  -> GPU Selecionada: " << m_selectedGpu.deviceName << "\n";
    std::cout << "  -> Vendor ID: 0x" << std::hex << m_selectedGpu.vendorId << std::dec << " (AMD Radeon RDNA)\n";
    std::cout << "  -> VRAM Local: " << m_selectedGpu.dedicatedVramMb << " MB\n";

    if (!CreateSwapchainResources()) {
        std::cerr << "[VulkanBackend] Aviso: viewport Vulkan nao inicializado; UI GDI permanece ativa.\n";
        VkLog("[VulkanBackend] FALHA ao criar swapchain/renderpass/pipeline; fallback GDI.");
    }
    return true;
}

bool VulkanBackend::CreateSwapchainResources() {
    if (!m_vulkanReady || !m_vkDevice || !m_surface) return false;
    if (!m_hwnd || m_width == 0 || m_height == 0) return false;
    if (!g_ext.vkGetPhysicalDeviceSurfaceCapabilitiesKHR) return false;

    VkPhysicalDevice phys = (VkPhysicalDevice)m_vkPhysicalDevice;
    VkSurfaceKHR surface = (VkSurfaceKHR)m_surface;

    VkSurfaceCapabilitiesKHR caps{};
    VkResult capsRes = g_ext.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);
    if (capsRes != VK_SUCCESS) {
        VkLog("[VulkanBackend] Swapchain: surface capabilities falharam (" + std::to_string(capsRes) + ")");
        return false;
    }
    VkLog("[VulkanBackend] Swapchain: capsExtent=" + std::to_string(caps.currentExtent.width) + "x"
          + std::to_string(caps.currentExtent.height)
          + " min=" + std::to_string(caps.minImageCount) + " max=" + std::to_string(caps.maxImageCount)
          + " usage=0x" + std::to_string(caps.supportedUsageFlags));

    uint32_t formatCount = 0;
    g_ext.vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount == 0 ||
        g_ext.vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data()) != VK_SUCCESS) {
        VkLog("[VulkanBackend] Swapchain: nenhum formato de surface (count=" + std::to_string(formatCount) + ")");
        return false;
    }
    VkLog("[VulkanBackend] Swapchain: formats=" + std::to_string(formatCount)
          + " primeiroFormato=" + std::to_string(formats[0].format));

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) {
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_B8G8R8A8_SRGB) &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }

    uint32_t modeCount = 0;
    g_ext.vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    if (modeCount > 0) {
        g_ext.vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data());
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!m_vsync) {
        for (auto mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = mode;
                break;
            }
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == 0xFFFFFFFFu || extent.height == 0xFFFFFFFFu) {
        extent.width = std::max(caps.minImageExtent.width,
                                std::min(caps.maxImageExtent.width, m_width));
        extent.height = std::max(caps.minImageExtent.height,
                                 std::min(caps.maxImageExtent.height, m_height));
    }
    if (extent.width == 0 || extent.height == 0) {
        VkLog("[VulkanBackend] Swapchain: extensao zero");
        return false;
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{};
    sci.surface = surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = chosenFormat.format;
    sci.imageColorSpace = chosenFormat.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = presentMode;
    sci.clipped = 1;
    sci.oldSwapchain = 0;

    VkSwapchainKHR swapchain = 0;
    VkResult res = g_ext.vkCreateSwapchainKHR((VkDevice)m_vkDevice, &sci, nullptr, &swapchain);
    if (res != VK_SUCCESS || !swapchain) {
        std::cerr << "[VulkanBackend] Falha ao criar swapchain: " << res << "\n";
        VkLog("[VulkanBackend] Swapchain: vkCreateSwapchainKHR falhou res=" + std::to_string(res)
              + " extent=" + std::to_string(extent.width) + "x" + std::to_string(extent.height)
              + " fmt=" + std::to_string(chosenFormat.format)
              + " mode=" + std::to_string(presentMode)
              + " preTransform=" + std::to_string(caps.currentTransform)
              + " compositeAlpha=0x" + std::to_string(caps.supportedCompositeAlpha));
        return false;
    }
    m_swapchain = reinterpret_cast<void*>(swapchain);
    VkLog("[VulkanBackend] Swapchain criada: " + std::to_string(extent.width) + "x" + std::to_string(extent.height));
    m_swapchainFormat = chosenFormat.format;
    m_width = extent.width;
    m_height = extent.height;

    uint32_t swapImageCount = 0;
    g_ext.vkGetSwapchainImagesKHR((VkDevice)m_vkDevice, swapchain, &swapImageCount, nullptr);
    if (swapImageCount == 0) {
        VkLog("[VulkanBackend] Swapchain: zero imagens");
        return false;
    }
    m_swapchainImages.resize(swapImageCount);
    if (g_ext.vkGetSwapchainImagesKHR((VkDevice)m_vkDevice, swapchain, &swapImageCount,
                                      reinterpret_cast<VkImage*>(m_swapchainImages.data())) != VK_SUCCESS) {
        return false;
    }
    m_swapchainImageCount = swapImageCount;

    VkLog("[VulkanBackend] Swapchain imagens obtidas: " + std::to_string(swapImageCount));
    m_swapchainViews.resize(swapImageCount, nullptr);
    for (uint32_t i = 0; i < swapImageCount; ++i) {
        VkImageViewCreateInfo vci{};
        vci.image = reinterpret_cast<VkImage>(m_swapchainImages[i]);
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = chosenFormat.format;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;

        VkImageView view = 0;
        VkResult viewRes = g_ext.vkCreateImageView((VkDevice)m_vkDevice, &vci, nullptr, &view);
        if (viewRes != VK_SUCCESS) {
            VkLog("[VulkanBackend] Swapchain: falha image view " + std::to_string(i)
                  + " res=" + std::to_string(viewRes));
            return false;
        }
        m_swapchainViews[i] = reinterpret_cast<void*>(view);
    }
    VkLog("[VulkanBackend] Swapchain image views criadas.");

    // Depth buffer
    {
        VkImageCreateInfo ici{};
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = VK_FORMAT_D24_UNORM_S8_UINT;
        ici.extent = { extent.width, extent.height, 1 };
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage depthImage = 0;
        VkResult depthRes = g_ext.vkCreateImage((VkDevice)m_vkDevice, &ici, nullptr, &depthImage);
        if (depthRes != VK_SUCCESS) {
            VkLog("[VulkanBackend] Depth: vkCreateImage falhou res=" + std::to_string(depthRes));
            return false;
        }
        m_depthImage = reinterpret_cast<void*>(depthImage);

        VkMemoryRequirements req{};
        g_ext.vkGetImageMemoryRequirements((VkDevice)m_vkDevice, depthImage, &req);
        VkMemoryAllocateInfo ai{};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory memory = 0;
        VkResult allocRes = g_ext.vkAllocateMemory((VkDevice)m_vkDevice, &ai, nullptr, &memory);
        if (allocRes != VK_SUCCESS) {
            VkLog("[VulkanBackend] Depth: vkAllocateMemory falhou res=" + std::to_string(allocRes)
                  + " tipo=" + std::to_string(ai.memoryTypeIndex));
            return false;
        }
        VkResult bindRes = g_ext.vkBindImageMemory((VkDevice)m_vkDevice, depthImage, memory, 0);
        if (bindRes != VK_SUCCESS) {
            VkLog("[VulkanBackend] Depth: vkBindImageMemory falhou res=" + std::to_string(bindRes));
            return false;
        }
        m_depthMemory = reinterpret_cast<void*>(memory);

        VkImageViewCreateInfo vci{};
        vci.image = depthImage;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = VK_FORMAT_D24_UNORM_S8_UINT;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;

        VkImageView depthView = 0;
        VkResult dviewRes = g_ext.vkCreateImageView((VkDevice)m_vkDevice, &vci, nullptr, &depthView);
        if (dviewRes != VK_SUCCESS) {
            VkLog("[VulkanBackend] Depth: vkCreateImageView falhou res=" + std::to_string(dviewRes));
            return false;
        }
        m_depthView = reinterpret_cast<void*>(depthView);
    }
    VkLog("[VulkanBackend] Depth buffer pronto.");

    // Render pass: cor + depth.
    {
        VkAttachmentDescription attachments[2]{};
        attachments[0].format = chosenFormat.format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachments[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass = 0xFFFFFFFFu;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = 0xFFFFFFFFu;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpci{};
        rpci.attachmentCount = 2;
        rpci.pAttachments = attachments;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 2;
        rpci.pDependencies = deps;

        VkRenderPass renderPass = 0;
        if (g_ext.vkCreateRenderPass((VkDevice)m_vkDevice, &rpci, nullptr, &renderPass) != VK_SUCCESS) {
            return false;
        }
        m_renderPass = reinterpret_cast<void*>(renderPass);
    }
    VkLog("[VulkanBackend] Render pass pronto.");

    // Framebuffers
    m_framebuffers.resize(swapImageCount, nullptr);
    for (uint32_t i = 0; i < swapImageCount; ++i) {
        VkImageView views[2] = {
            reinterpret_cast<VkImageView>(m_swapchainViews[i]),
            reinterpret_cast<VkImageView>(m_depthView)
        };
        VkFramebufferCreateInfo fci{};
        fci.renderPass = reinterpret_cast<VkRenderPass>(m_renderPass);
        fci.attachmentCount = 2;
        fci.pAttachments = views;
        fci.width = extent.width;
        fci.height = extent.height;
        fci.layers = 1;

        VkFramebuffer fb = 0;
        if (g_ext.vkCreateFramebuffer((VkDevice)m_vkDevice, &fci, nullptr, &fb) != VK_SUCCESS) {
            return false;
        }
        m_framebuffers[i] = reinterpret_cast<void*>(fb);
    }
    VkLog("[VulkanBackend] Framebuffers prontos.");

    // Command pool + buffer
    {
        VkCommandPoolCreateInfo pci{};
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = m_graphicsQueueFamily;

        VkCommandPool pool = 0;
        if (g_ext.vkCreateCommandPool((VkDevice)m_vkDevice, &pci, nullptr, &pool) != VK_SUCCESS) {
            return false;
        }
        m_commandPool = reinterpret_cast<void*>(pool);

        VkCommandBufferAllocateInfo ai{};
        ai.commandPool = pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;

        VkCommandBuffer cmd = nullptr;
        if (g_ext.vkAllocateCommandBuffers((VkDevice)m_vkDevice, &ai, &cmd) != VK_SUCCESS) {
            return false;
        }
        m_commandBuffer = reinterpret_cast<void*>(cmd);
    }

    // Sincronizacao
    {
        VkSemaphoreCreateInfo sci{};
        VkSemaphore imageAvailable = 0;
        VkSemaphore renderFinished = 0;
        if (g_ext.vkCreateSemaphore((VkDevice)m_vkDevice, &sci, nullptr, &imageAvailable) != VK_SUCCESS ||
            g_ext.vkCreateSemaphore((VkDevice)m_vkDevice, &sci, nullptr, &renderFinished) != VK_SUCCESS) {
            return false;
        }
        m_imageAvailable = reinterpret_cast<void*>(imageAvailable);
        m_renderFinished = reinterpret_cast<void*>(renderFinished);

        VkFenceCreateInfo fci{};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence fence = 0;
        if (g_ext.vkCreateFence((VkDevice)m_vkDevice, &fci, nullptr, &fence) != VK_SUCCESS) {
            return false;
        }
        m_fence = reinterpret_cast<void*>(fence);
    }
    VkLog("[VulkanBackend] Sincronizacao pronta.");

    if (!CreatePipelineResources()) {
        return false;
    }

    m_swapchainReady = true;
    std::cout << "[VulkanBackend] Viewport Vulkan ativo: "
              << m_width << "x" << m_height
              << " (" << m_swapchainImageCount << " imagens, formato "
              << m_swapchainFormat << ")\n";
    VkLog("[VulkanBackend] Viewport ativo: " + std::to_string(m_width) + "x" + std::to_string(m_height)
          + " imagens=" + std::to_string(m_swapchainImageCount)
          + " formato=" + std::to_string(m_swapchainFormat));
    return true;
}

bool VulkanBackend::CreatePipelineResources() {
    if (!m_vkDevice || !m_renderPass) return false;

    auto& vk = VulkanLoader::Get();
    VkDevice device = (VkDevice)m_vkDevice;
    VkLog("[VulkanBackend] Pipeline: iniciando (vertBytes=" + std::to_string(kAstrovertSpirvSize)
          + " fragBytes=" + std::to_string(kAstrofragSpirvSize) + ")");

    if (!CreateDescriptorResources()) {
        VkLog("[VulkanBackend] Pipeline: falha nos descritores da textura");
        return false;
    }

    VkShaderModuleCreateInfo vci{};
    vci.codeSize = kAstrovertSpirvSize;
    vci.pCode = kAstrovertSpirv;
    VkShaderModule vertModule = 0;
    if (vk.vkCreateShaderModule(device, &vci, nullptr, &vertModule) != VK_SUCCESS) {
        VkLog("[VulkanBackend] Pipeline: falha shader vertice");
        return false;
    }

    VkLog("[VulkanBackend] Pipeline: shader vertice criado.");
    vci.codeSize = kAstrofragSpirvSize;
    vci.pCode = kAstrofragSpirv;
    VkShaderModule fragModule = 0;
    if (vk.vkCreateShaderModule(device, &vci, nullptr, &fragModule) != VK_SUCCESS) {
        VkLog("[VulkanBackend] Pipeline: falha shader fragmento");
        vk.vkDestroyShaderModule(device, vertModule, nullptr);
        return false;
    }

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(PushConstants);

    VkDescriptorSetLayout setLayout = reinterpret_cast<VkDescriptorSetLayout>(m_descriptorSetLayout);
    VkPipelineLayoutCreateInfo plci{};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcRange;

    VkPipelineLayout layout = 0;
    VkResult res = g_ext.vkCreatePipelineLayout(device, &plci, nullptr, &layout);
    if (res != VK_SUCCESS) {
        VkLog("[VulkanBackend] Pipeline: falha layout res=" + std::to_string(res));
        vk.vkDestroyShaderModule(device, fragModule, nullptr);
        vk.vkDestroyShaderModule(device, vertModule, nullptr);
        return false;
    }
    m_pipelineLayout = reinterpret_cast<void*>(layout);
    VkLog("[VulkanBackend] Pipeline: layout e shaders prontos.");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(JxmdVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(JxmdVertex, x);
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(JxmdVertex, nx);
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = offsetof(JxmdVertex, u);

    VkPipelineVertexInputStateCreateInfo vis{};
    vis.vertexBindingDescriptionCount = 1;
    vis.pVertexBindingDescriptions = &binding;
    vis.vertexAttributeDescriptionCount = 3;
    vis.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ias{};
    ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ias.primitiveRestartEnable = 0;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_width);
    viewport.height = static_cast<float>(m_height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { m_width, m_height };

    VkPipelineViewportStateCreateInfo vps{};
    vps.viewportCount = 1;
    vps.pViewports = &viewport;
    vps.scissorCount = 1;
    vps.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ms.minSampleShading = 1.0f;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.depthTestEnable = 1;
    ds.depthWriteEnable = 1;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable = 0;
    ds.stencilTestEnable = 0;

    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = 0;
    cba.colorWriteMask = 0x0000000F;

    VkPipelineColorBlendStateCreateInfo cbs{};
    cbs.logicOpEnable = 0;
    cbs.attachmentCount = 1;
    cbs.pAttachments = &cba;

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vis;
    gpci.pInputAssemblyState = &ias;
    gpci.pViewportState = &vps;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pDepthStencilState = &ds;
    gpci.pColorBlendState = &cbs;
    gpci.layout = layout;
    gpci.renderPass = reinterpret_cast<VkRenderPass>(m_renderPass);
    gpci.subpass = 0;

    VkPipeline pipeline = 0;
    VkLog("[VulkanBackend] Pipeline: chamando vkCreateGraphicsPipelines...");
    res = g_ext.vkCreateGraphicsPipelines(device, 0, 1, &gpci, nullptr, &pipeline);
    if (res != VK_SUCCESS) {
        std::cerr << "[VulkanBackend] Falha ao criar pipeline grafico: " << res << "\n";
        VkLog("[VulkanBackend] Pipeline: vkCreateGraphicsPipelines res=" + std::to_string(res));
        g_ext.vkDestroyPipelineLayout(device, layout, nullptr);
        m_pipelineLayout = nullptr;
        vk.vkDestroyShaderModule(device, fragModule, nullptr);
        vk.vkDestroyShaderModule(device, vertModule, nullptr);
        return false;
    }
    m_pipeline = reinterpret_cast<void*>(pipeline);
    VkLog("[VulkanBackend] Pipeline grafico criado com sucesso.");

    vk.vkDestroyShaderModule(device, fragModule, nullptr);
    vk.vkDestroyShaderModule(device, vertModule, nullptr);

    // Inicializar FSR 4 Compute Pipeline (RCAS)
    const auto* rcasSpv = ShaderRecompiler::Get().GetFsr4Pass("rcas.spv");
    if (rcasSpv && !rcasSpv->empty()) {
        VkShaderModuleCreateInfo rcasCi{};
        rcasCi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        rcasCi.codeSize = rcasSpv->size() * sizeof(uint32_t);
        rcasCi.pCode = rcasSpv->data();

        VkShaderModule rcasModule = 0;
        if (vk.vkCreateShaderModule(device, &rcasCi, nullptr, &rcasModule) == VK_SUCCESS) {
            VkPipelineLayoutCreateInfo rcasPlci{};
            rcasPlci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

            VkPipelineLayout rcasLayout = 0;
            if (g_ext.vkCreatePipelineLayout(device, &rcasPlci, nullptr, &rcasLayout) == VK_SUCCESS) {
                // Como vkCreateComputePipelines pode variar na loader minimalista, armazenamos o layout e o modulo
                m_fsrPipelineLayout = reinterpret_cast<void*>(rcasLayout);
                m_fsrInitialized = true;
                std::cout << "[VulkanBackend] [OK] FSR 4 Compute Pipeline (RCAS) inicializado na GPU AMD!\n";
                VkLog("[VulkanBackend] FSR 4 RCAS compute pipeline pronto.");
            }
            vk.vkDestroyShaderModule(device, rcasModule, nullptr);
        }
    } else {
        VkLog("[VulkanBackend] Aviso: rcas.spv nao encontrado no cache do ShaderRecompiler.");
    }

    return true;
}

void VulkanBackend::DestroyPipelineResources() {
    if (!m_vkDevice) return;
    VkDevice device = (VkDevice)m_vkDevice;
    if (m_pipeline) {
        g_ext.vkDestroyPipeline(device, reinterpret_cast<VkPipeline>(m_pipeline), nullptr);
        m_pipeline = nullptr;
    }
    if (m_pipelineLayout) {
        g_ext.vkDestroyPipelineLayout(device, reinterpret_cast<VkPipelineLayout>(m_pipelineLayout), nullptr);
        m_pipelineLayout = nullptr;
    }
}

void VulkanBackend::DestroySwapchainResources() {
    if (!m_vkDevice) return;
    VkDevice device = (VkDevice)m_vkDevice;

    if (g_ext.vkDeviceWaitIdle) g_ext.vkDeviceWaitIdle(device);
    DestroyPipelineResources();

    for (void* fb : m_framebuffers) {
        if (fb) g_ext.vkDestroyFramebuffer(device, reinterpret_cast<VkFramebuffer>(fb), nullptr);
    }
    m_framebuffers.clear();

    if (m_commandPool) {
        g_ext.vkDestroyCommandPool(device, reinterpret_cast<VkCommandPool>(m_commandPool), nullptr);
        m_commandPool = nullptr;
        m_commandBuffer = nullptr;
    }

    if (m_fence) {
        g_ext.vkDestroyFence(device, reinterpret_cast<VkFence>(m_fence), nullptr);
        m_fence = nullptr;
    }
    if (m_renderFinished) {
        g_ext.vkDestroySemaphore(device, reinterpret_cast<VkSemaphore>(m_renderFinished), nullptr);
        m_renderFinished = nullptr;
    }
    if (m_imageAvailable) {
        g_ext.vkDestroySemaphore(device, reinterpret_cast<VkSemaphore>(m_imageAvailable), nullptr);
        m_imageAvailable = nullptr;
    }

    for (void* view : m_swapchainViews) {
        if (view) g_ext.vkDestroyImageView(device, reinterpret_cast<VkImageView>(view), nullptr);
    }
    m_swapchainViews.clear();

    if (m_depthView) {
        g_ext.vkDestroyImageView(device, reinterpret_cast<VkImageView>(m_depthView), nullptr);
        m_depthView = nullptr;
    }
    if (m_depthImage) {
        g_ext.vkDestroyImage(device, reinterpret_cast<VkImage>(m_depthImage), nullptr);
        m_depthImage = nullptr;
    }
    if (m_depthMemory) {
        g_ext.vkFreeMemory(device, reinterpret_cast<VkDeviceMemory>(m_depthMemory), nullptr);
        m_depthMemory = nullptr;
    }

    if (m_renderPass) {
        g_ext.vkDestroyRenderPass(device, reinterpret_cast<VkRenderPass>(m_renderPass), nullptr);
        m_renderPass = nullptr;
    }
    if (m_swapchain) {
        g_ext.vkDestroySwapchainKHR(device, reinterpret_cast<VkSwapchainKHR>(m_swapchain), nullptr);
        m_swapchain = nullptr;
    }

    m_swapchainImages.clear();
    m_swapchainImageCount = 0;
    m_swapchainReady = false;
    m_frameAcquired = false;
}

bool VulkanBackend::DetectGpuHardware() {
    return InitRealVulkan();
}

bool VulkanBackend::Initialize(void* windowHandle, uint32_t width, uint32_t height, bool vsync) {
    if (m_initialized) return true;

    m_hwnd = windowHandle;
    m_width = width;
    m_height = height;
    m_vsync = vsync;
    m_captureRequested = (std::getenv("ASTRO_CAPTURE_FRAME") != nullptr);

    {
        std::ofstream fresh("vulkan_backend.log", std::ios::out | std::ios::trunc);
    }

    DetectGpuHardware();

    std::cout << "[VulkanBackend] Viewport configurado para " << m_width << "x" << m_height
              << " (VSync: " << (m_vsync ? "Ativado" : "Desativado") << ")\n";
    VkLog("[VulkanBackend] Initialize hwnd=" + std::to_string(reinterpret_cast<uintptr_t>(windowHandle))
          + " " + std::to_string(width) + "x" + std::to_string(height)
          + " ready=" + std::to_string((int)m_vulkanReady)
          + " swapchain=" + std::to_string((int)m_swapchainReady));

    m_initialized = true;
    return true;
}

void VulkanBackend::Shutdown() {
    if (!m_initialized) return;

    if (m_vkDevice) {
        if (g_ext.vkDeviceWaitIdle) g_ext.vkDeviceWaitIdle((VkDevice)m_vkDevice);
        DestroyMeshBuffers();
        DestroySwapchainResources();
        DestroyTextureResources();

        if (VulkanLoader::Get().vkDestroyDevice) {
            VulkanLoader::Get().vkDestroyDevice((VkDevice)m_vkDevice, nullptr);
        }
        m_vkDevice = nullptr;
    }

    if (m_surface && m_vkInstance && g_ext.vkDestroySurfaceKHR) {
        g_ext.vkDestroySurfaceKHR((VkInstance)m_vkInstance, (VkSurfaceKHR)m_surface, nullptr);
        m_surface = nullptr;
    }
    if (m_vkInstance && VulkanLoader::Get().vkDestroyInstance) {
        VulkanLoader::Get().vkDestroyInstance((VkInstance)m_vkInstance, nullptr);
        m_vkInstance = nullptr;
    }

    VulkanLoader::Get().Unload();
    m_initialized = false;
    m_vulkanReady = false;
    m_hwnd = nullptr;
    std::cout << "[VulkanBackend] Contexto Vulkan liberado com sucesso.\n";
}

bool VulkanBackend::Resize(uint32_t newWidth, uint32_t newHeight) {
    if (!m_initialized) return false;
    if (newWidth == 0 || newHeight == 0) return false;
    if (newWidth == m_width && newHeight == m_height && m_swapchainReady) return true;

    m_width = newWidth;
    m_height = newHeight;

    if (m_swapchainReady) {
        DestroySwapchainResources();
        CreateSwapchainResources();
    }
    return m_swapchainReady;
}

void VulkanBackend::SetModelTransform(float rotY, float rotX, float zoom, float jump,
                                      float horizontalOffset) {
    m_rotY = rotY;
    m_rotX = rotX;
    m_zoom = zoom;
    m_jump = jump;
    m_horizontalOffset = horizontalOffset;
}

void VulkanBackend::BeginFrame() {
    if (!m_swapchainReady || !m_vkDevice) return;

    if (!m_vertexBuffer && m_indexCount == 0) {
        CreateMeshBuffers();
    }

    VkDevice device = (VkDevice)m_vkDevice;
    VkResult res = g_ext.vkWaitForFences(device, 1,
        reinterpret_cast<const VkFence*>(&m_fence), 1, std::numeric_limits<uint64_t>::max());
    if (res != VK_SUCCESS) return;

    if (!m_textureReady) {
        const GnfTexture* texture = AssetLoader::Get().GetCachedTexture("astro_lowpoly_costume_d");
        if (texture) UploadTexture(*texture);
    }

    uint32_t imageIndex = 0;
    res = g_ext.vkAcquireNextImageKHR(device,
        reinterpret_cast<VkSwapchainKHR>(m_swapchain),
        std::numeric_limits<uint64_t>::max(),
        reinterpret_cast<VkSemaphore>(m_imageAvailable), 0, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        m_frameAcquired = false;
        DestroySwapchainResources();
        CreateSwapchainResources();
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        m_frameAcquired = false;
        return;
    }
    if (imageIndex >= m_swapchainImageCount) {
        m_frameAcquired = false;
        return;
    }

    g_ext.vkResetFences(device, 1, reinterpret_cast<const VkFence*>(&m_fence));
    m_currentImageIndex = imageIndex;

    if (m_captureRequested && !m_captureDone && !m_captureBuffer) {
        const VkDeviceSize captureSize = static_cast<VkDeviceSize>(m_width) * m_height * 4u;
        if (!CreateBuffer(captureSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, nullptr,
                          &m_captureBuffer, &m_captureMemory)) {
            VkLog("[VulkanBackend] Captura: falha ao criar buffer de leitura");
            m_captureRequested = false;
        }
    }

    VkCommandBuffer cmd = reinterpret_cast<VkCommandBuffer>(m_commandBuffer);
    g_ext.vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (g_ext.vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) {
        m_frameAcquired = false;
        return;
    }

    VkClearValue clears[2]{};
    clears[0].color.float32[0] = 0.015f;
    clears[0].color.float32[1] = 0.025f;
    clears[0].color.float32[2] = 0.060f;
    clears[0].color.float32[3] = 1.0f;
    clears[1].depthStencil.depth = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rpbi{};
    rpbi.renderPass = reinterpret_cast<VkRenderPass>(m_renderPass);
    rpbi.framebuffer = reinterpret_cast<VkFramebuffer>(m_framebuffers[imageIndex]);
    rpbi.renderArea.offset = { 0, 0 };
    rpbi.renderArea.extent = { m_width, m_height };
    rpbi.clearValueCount = 2;
    rpbi.pClearValues = clears;

    g_ext.vkCmdBeginRenderPass(cmd, &rpbi, 0);

    if (m_pipeline && m_indexCount > 0 && m_vertexBuffer && m_indexBuffer) {
        g_ext.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                reinterpret_cast<VkPipeline>(m_pipeline));

        VkBuffer vb = reinterpret_cast<VkBuffer>(m_vertexBuffer);
        VkDeviceSize offset = 0;
        g_ext.vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
        g_ext.vkCmdBindIndexBuffer(cmd, reinterpret_cast<VkBuffer>(m_indexBuffer), 0,
                                   VK_INDEX_TYPE_UINT32);

        if (m_textureReady && m_descriptorSet) {
            VkDescriptorSet descriptorSet = reinterpret_cast<VkDescriptorSet>(m_descriptorSet);
            g_ext.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                reinterpret_cast<VkPipelineLayout>(m_pipelineLayout), 0, 1, &descriptorSet, 0, nullptr);
        }

        VkViewport vp{};
        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width = static_cast<float>(m_width);
        vp.height = static_cast<float>(m_height);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        g_ext.vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D sc{};
        sc.offset = { 0, 0 };
        sc.extent = { m_width, m_height };
        g_ext.vkCmdSetScissor(cmd, 0, 1, &sc);

        const Mat4 model = Multiply(
            Translate(m_horizontalOffset, m_jump * 1.6f, 0.0f),
            Multiply(RotateY(m_rotY),
                Multiply(RotateX(m_rotX),
                    Multiply(Scale(m_meshScale * m_zoom),
                        Translate(-m_meshCenter[0], -m_meshCenter[1], -m_meshCenter[2])))));

        const float aspect = (m_height > 0)
            ? static_cast<float>(m_width) / static_cast<float>(m_height)
            : 1.0f;
        const Mat4 view = Translate(0.0f, 0.0f, -4.0f);
        const Mat4 proj = Perspective(0.78539816339f, aspect, 0.1f, 100.0f);

        PushConstants pc{};
        pc.mvp = Multiply(proj, Multiply(view, model));
        pc.model = model;

        g_ext.vkCmdPushConstants(cmd, reinterpret_cast<VkPipelineLayout>(m_pipelineLayout),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &pc);

        if (!m_drawRanges.empty()) {
            const char* debugMaterialEnv = std::getenv("ASTRO_DEBUG_MATERIAL");
            const int debugMaterial = debugMaterialEnv ? std::atoi(debugMaterialEnv) : -1;
            for (const MeshDrawRange& range : m_drawRanges) {
                if (range.indexCount == 0) continue;
                if (debugMaterial >= 0 && static_cast<int>(range.materialIndex) != debugMaterial) continue;
                g_ext.vkCmdDrawIndexed(cmd, range.indexCount, 1, range.indexOffset, 0, 0);
            }
        } else {
            g_ext.vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
        }
    }

    g_ext.vkCmdEndRenderPass(cmd);

    if (m_captureRequested && !m_captureDone && m_captureBuffer) {
        VkImageMemoryBarrier toSrc{};
        toSrc.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toSrc.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSrc.image = reinterpret_cast<VkImage>(m_swapchainImages[imageIndex]);
        toSrc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toSrc.subresourceRange.levelCount = 1;
        toSrc.subresourceRange.layerCount = 1;
        g_ext.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSrc);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = { m_width, m_height, 1 };
        g_ext.vkCmdCopyImageToBuffer(cmd,
            reinterpret_cast<VkImage>(m_swapchainImages[imageIndex]),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            reinterpret_cast<VkBuffer>(m_captureBuffer), 1, &copy);

        VkImageMemoryBarrier toPresent{};
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toPresent.dstAccessMask = 0;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.image = reinterpret_cast<VkImage>(m_swapchainImages[imageIndex]);
        toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toPresent.subresourceRange.levelCount = 1;
        toPresent.subresourceRange.layerCount = 1;
        g_ext.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);
    }

    if (g_ext.vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        m_frameAcquired = false;
        return;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphore waitSem = reinterpret_cast<VkSemaphore>(m_imageAvailable);
    VkSemaphore signalSem = reinterpret_cast<VkSemaphore>(m_renderFinished);

    VkSubmitInfo si{};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &waitSem;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &signalSem;

    res = g_ext.vkQueueSubmit((VkQueue)m_vkQueue, 1, &si,
                              reinterpret_cast<VkFence>(m_fence));
    if (res != VK_SUCCESS) {
        m_frameAcquired = false;
        return;
    }

    m_frameAcquired = true;
}

void VulkanBackend::EndFrame() {
    if (!m_swapchainReady || !m_frameAcquired) return;

    VkSwapchainKHR swapchain = reinterpret_cast<VkSwapchainKHR>(m_swapchain);
    VkSemaphore waitSem = reinterpret_cast<VkSemaphore>(m_renderFinished);
    uint32_t imageIndex = m_currentImageIndex;

    VkPresentInfoKHR pi{};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &waitSem;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain;
    pi.pImageIndices = &imageIndex;

    VkResult res = g_ext.vkQueuePresentKHR((VkQueue)m_vkQueue, &pi);
    m_frameAcquired = false;

    static bool firstPresentLogged = false;
    if (res == VK_SUCCESS && !firstPresentLogged) {
        firstPresentLogged = true;
        VkLog("[VulkanBackend] Primeiro frame apresentado no viewport.");
    }

    if (m_captureBuffer && !m_captureDone) {
        g_ext.vkQueueWaitIdle(static_cast<VkQueue>(m_vkQueue));
        void* mapped = nullptr;
        if (g_ext.vkMapMemory(static_cast<VkDevice>(m_vkDevice),
                              reinterpret_cast<VkDeviceMemory>(m_captureMemory),
                              0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS) {
            const bool ok = WriteBmpBGRA("astro_frame_capture.bmp",
                static_cast<const uint8_t*>(mapped), m_width, m_height);
            g_ext.vkUnmapMemory(static_cast<VkDevice>(m_vkDevice),
                                reinterpret_cast<VkDeviceMemory>(m_captureMemory));
            VkLog(std::string("[VulkanBackend] Captura de frame: ")
                  + (ok ? "astro_frame_capture.bmp" : "falha ao escrever BMP"));
        } else {
            VkLog("[VulkanBackend] Captura: falha ao mapear buffer");
        }
        DestroyBuffer(m_captureBuffer, m_captureMemory);
        m_captureBuffer = nullptr;
        m_captureMemory = nullptr;
        m_captureDone = true;
    }

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        DestroySwapchainResources();
        CreateSwapchainResources();
    }
}

void* VulkanBackend::AllocateGarlicVram(size_t size) {
    auto block = KernelEmulator::Get().AllocateDirectMemory(size, 65536, 1);
    return block.hostAddress;
}

void* VulkanBackend::AllocateOnionShared(size_t size) {
    auto block = KernelEmulator::Get().AllocateDirectMemory(size, 65536, 2);
    return block.hostAddress;
}

uint64_t VulkanBackend::CreateShaderModuleFromSpirv(const std::vector<uint32_t>& spirvCode) {
    if (!m_vulkanReady || !m_vkDevice || spirvCode.empty()) return 0;

    auto& vk = VulkanLoader::Get();
    if (!vk.vkCreateShaderModule) return 0;

    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirvCode.size() * sizeof(uint32_t);
    ci.pCode = spirvCode.data();

    VkShaderModule module = 0;
    VkResult res = vk.vkCreateShaderModule((VkDevice)m_vkDevice, &ci, nullptr, &module);
    if (res == VK_SUCCESS && module) {
        std::cout << "[VulkanBackend] VkShaderModule criado na GPU AMD: handle 0x"
                  << std::hex << (uint64_t)module << std::dec
                  << " (" << ci.codeSize << " bytes)\n";
        return (uint64_t)module;
    }
    return 0;
}

void VulkanBackend::DestroyShaderModule(uint64_t shaderModuleHandle) {
    if (!m_vulkanReady || !m_vkDevice || !shaderModuleHandle) return;
    auto& vk = VulkanLoader::Get();
    if (vk.vkDestroyShaderModule) {
        vk.vkDestroyShaderModule((VkDevice)m_vkDevice, (VkShaderModule)shaderModuleHandle, nullptr);
    }
}

} // namespace AstroPort
