#pragma once

// Minimal Vulkan 1.0/1.3 type subset used by the Win32 swapchain viewport.
// The project loads vulkan-1.dll dynamically and does not depend on the SDK.

#include "vulkan_loader.h"

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace AstroPort {

using VkSemaphore = uint64_t;
using VkFence = uint64_t;
using VkImageAspectFlags = uint32_t;
using VkImageUsageFlags = uint32_t;
using VkSurfaceTransformFlagsKHR = uint32_t;
using VkCompositeAlphaFlagsKHR = uint32_t;
using VkColorComponentFlags = uint32_t;
using VkPipelineStageFlags = uint32_t;
using VkShaderStageFlags = uint32_t;
using VkAccessFlags = uint32_t;
using VkBufferUsageFlags = uint32_t;
using VkMemoryPropertyFlags = uint32_t;

#ifndef VK_WHOLE_SIZE
#define VK_WHOLE_SIZE (~0ULL)
#endif

#ifndef VK_QUEUE_FAMILY_IGNORED
#define VK_QUEUE_FAMILY_IGNORED (~0U)
#endif

enum VkFormat : uint32_t {
    VK_FORMAT_UNDEFINED = 0,
    VK_FORMAT_R32G32_SFLOAT = 103,
    VK_FORMAT_R32G32B32_SFLOAT = 106,
    VK_FORMAT_B8G8R8A8_UNORM = 44,
    VK_FORMAT_B8G8R8A8_SRGB = 50,
    VK_FORMAT_D16_UNORM = 124,
    VK_FORMAT_D32_SFLOAT = 126,
    VK_FORMAT_D24_UNORM_S8_UINT = 130,
    VK_FORMAT_BC4_UNORM_BLOCK = 139,
    VK_FORMAT_BC7_UNORM_BLOCK = 145,
    VK_FORMAT_BC7_SRGB_BLOCK = 146
};

enum VkColorSpaceKHR : uint32_t {
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR = 0
};

enum VkPresentModeKHR : uint32_t {
    VK_PRESENT_MODE_IMMEDIATE_KHR = 0,
    VK_PRESENT_MODE_MAILBOX_KHR = 1,
    VK_PRESENT_MODE_FIFO_KHR = 2,
    VK_PRESENT_MODE_FIFO_RELAXED_KHR = 3
};

enum VkSurfaceTransformFlagBitsKHR : uint32_t {
    VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR = 0x00000001,
    VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR = 0x00000002,
    VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR = 0x00000004,
    VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR = 0x00000008
};

enum VkCompositeAlphaFlagBitsKHR : uint32_t {
    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR = 0x00000001
};

enum VkImageUsageFlagBits : uint32_t {
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002,
    VK_IMAGE_USAGE_SAMPLED_BIT = 0x00000004,
    VK_IMAGE_USAGE_STORAGE_BIT = 0x00000008,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000020
};

enum VkImageAspectFlagBits : uint32_t {
    VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001,
    VK_IMAGE_ASPECT_DEPTH_BIT = 0x00000002
};

enum VkSharingMode : uint32_t {
    VK_SHARING_MODE_EXCLUSIVE = 0,
    VK_SHARING_MODE_CONCURRENT = 1
};

enum VkImageType : uint32_t {
    VK_IMAGE_TYPE_1D = 0,
    VK_IMAGE_TYPE_2D = 1,
    VK_IMAGE_TYPE_3D = 2
};

enum VkImageViewType : uint32_t {
    VK_IMAGE_VIEW_TYPE_1D = 0,
    VK_IMAGE_VIEW_TYPE_2D = 1,
    VK_IMAGE_VIEW_TYPE_3D = 2,
    VK_IMAGE_VIEW_TYPE_CUBE = 3
};

enum VkImageTiling : uint32_t {
    VK_IMAGE_TILING_OPTIMAL = 0,
    VK_IMAGE_TILING_LINEAR = 1
};

enum VkImageLayout : uint32_t {
    VK_IMAGE_LAYOUT_UNDEFINED = 0,
    VK_IMAGE_LAYOUT_GENERAL = 1,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002
};

enum VkFilter : uint32_t {
    VK_FILTER_NEAREST = 0,
    VK_FILTER_LINEAR = 1
};

enum VkSamplerMipmapMode : uint32_t {
    VK_SAMPLER_MIPMAP_MODE_NEAREST = 0,
    VK_SAMPLER_MIPMAP_MODE_LINEAR = 1
};

enum VkSamplerAddressMode : uint32_t {
    VK_SAMPLER_ADDRESS_MODE_REPEAT = 0,
    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT = 1,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE = 2,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER = 3
};

enum VkDescriptorType : uint32_t {
    VK_DESCRIPTOR_TYPE_SAMPLER = 0,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1
};

enum VkSampleCountFlagBits : uint32_t {
    VK_SAMPLE_COUNT_1_BIT = 0x00000001
};

enum VkAttachmentLoadOp : uint32_t {
    VK_ATTACHMENT_LOAD_OP_LOAD = 0,
    VK_ATTACHMENT_LOAD_OP_CLEAR = 1,
    VK_ATTACHMENT_LOAD_OP_DONT_CARE = 2
};

enum VkAttachmentStoreOp : uint32_t {
    VK_ATTACHMENT_STORE_OP_STORE = 0,
    VK_ATTACHMENT_STORE_OP_DONT_CARE = 1
};

enum VkPipelineBindPoint : uint32_t {
    VK_PIPELINE_BIND_POINT_GRAPHICS = 0,
    VK_PIPELINE_BIND_POINT_COMPUTE = 1
};

enum VkShaderStageFlagBits : uint32_t {
    VK_SHADER_STAGE_VERTEX_BIT = 0x00000001,
    VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010
};

enum VkPrimitiveTopology : uint32_t {
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3
};

enum VkVertexInputRate : uint32_t {
    VK_VERTEX_INPUT_RATE_VERTEX = 0,
    VK_VERTEX_INPUT_RATE_INSTANCE = 1
};

enum VkPolygonMode : uint32_t {
    VK_POLYGON_MODE_FILL = 0,
    VK_POLYGON_MODE_LINE = 1
};

enum VkCullModeFlagBits : uint32_t {
    VK_CULL_MODE_NONE = 0,
    VK_CULL_MODE_FRONT_BIT = 0x00000001,
    VK_CULL_MODE_BACK_BIT = 0x00000002
};

enum VkFrontFace : uint32_t {
    VK_FRONT_FACE_COUNTER_CLOCKWISE = 0,
    VK_FRONT_FACE_CLOCKWISE = 1
};

enum VkCompareOp : uint32_t {
    VK_COMPARE_OP_NEVER = 0,
    VK_COMPARE_OP_LESS = 1,
    VK_COMPARE_OP_EQUAL = 2,
    VK_COMPARE_OP_LESS_OR_EQUAL = 3,
    VK_COMPARE_OP_ALWAYS = 7
};

enum VkStencilOp : uint32_t {
    VK_STENCIL_OP_KEEP = 0
};

enum VkLogicOp : uint32_t {
    VK_LOGIC_OP_CLEAR = 0,
    VK_LOGIC_OP_COPY = 3
};

enum VkBlendFactor : uint32_t {
    VK_BLEND_FACTOR_ZERO = 0,
    VK_BLEND_FACTOR_ONE = 1,
    VK_BLEND_FACTOR_SRC_ALPHA = 6,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 7
};

enum VkBlendOp : uint32_t {
    VK_BLEND_OP_ADD = 0
};

enum VkPipelineStageFlagBits : uint32_t {
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400,
    VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000
};

enum VkAccessFlagBits : uint32_t {
    VK_ACCESS_SHADER_READ_BIT = 0x00000020,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = 0x00000100,
    VK_ACCESS_TRANSFER_READ_BIT = 0x00000800,
    VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000
};

enum VkBufferUsageFlagBits : uint32_t {
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x00000001,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT = 0x00000002,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000040,
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000080
};

enum VkIndexType : uint32_t {
    VK_INDEX_TYPE_UINT16 = 0,
    VK_INDEX_TYPE_UINT32 = 1
};

enum VkCommandBufferLevel : uint32_t {
    VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0
};

enum VkCommandBufferUsageFlagBits : uint32_t {
    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001
};

enum VkCommandPoolCreateFlagBits : uint32_t {
    VK_COMMAND_POOL_CREATE_TRANSIENT_BIT = 0x00000001,
    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002
};

enum VkFenceCreateFlagBits : uint32_t {
    VK_FENCE_CREATE_SIGNALED_BIT = 0x00000001
};

enum VkMemoryPropertyFlagBits : uint32_t {
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002,
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004
};

struct VkExtent2D {
    uint32_t width = 0;
    uint32_t height = 0;
};

struct VkExtent3D {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
};

struct VkOffset2D {
    int32_t x = 0;
    int32_t y = 0;
};

struct VkOffset3D {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
};

struct VkRect2D {
    VkOffset2D offset;
    VkExtent2D extent;
};

struct VkSurfaceFormatKHR {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
};

struct VkSurfaceCapabilitiesKHR {
    uint32_t minImageCount = 0;
    uint32_t maxImageCount = 0;
    VkExtent2D currentExtent;
    VkExtent2D minImageExtent;
    VkExtent2D maxImageExtent;
    uint32_t maxImageArrayLayers = 0;
    VkSurfaceTransformFlagsKHR supportedTransforms = 0;
    VkSurfaceTransformFlagBitsKHR currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkCompositeAlphaFlagsKHR supportedCompositeAlpha = 0;
    VkImageUsageFlags supportedUsageFlags = 0;
};

struct VkWin32SurfaceCreateInfoKHR {
    VkStructureType sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    HINSTANCE hinstance = nullptr;
    HWND hwnd = nullptr;
};

struct VkSwapchainCreateInfoKHR {
    VkStructureType sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkSurfaceKHR surface = 0;
    uint32_t minImageCount = 0;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D imageExtent;
    uint32_t imageArrayLayers = 1;
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkSharingMode imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t queueFamilyIndexCount = 0;
    const uint32_t* pQueueFamilyIndices = nullptr;
    VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkBool32 clipped = 1;
    VkSwapchainKHR oldSwapchain = 0;
};

struct VkComponentMapping {
    uint32_t r = 0;
    uint32_t g = 0;
    uint32_t b = 0;
    uint32_t a = 0;
};

struct VkImageSubresourceRange {
    VkImageAspectFlags aspectMask = 0;
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
};

struct VkImageViewCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkImage image = 0;
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkComponentMapping components;
    VkImageSubresourceRange subresourceRange;
};

struct VkAttachmentDescription {
    VkFlags flags = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
};

struct VkAttachmentReference {
    uint32_t attachment = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct VkSubpassDescription {
    VkFlags flags = 0;
    VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    uint32_t inputAttachmentCount = 0;
    const VkAttachmentReference* pInputAttachments = nullptr;
    uint32_t colorAttachmentCount = 0;
    const VkAttachmentReference* pColorAttachments = nullptr;
    const VkAttachmentReference* pResolveAttachments = nullptr;
    const VkAttachmentReference* pDepthStencilAttachment = nullptr;
    uint32_t preserveAttachmentCount = 0;
    const uint32_t* pPreserveAttachments = nullptr;
};

struct VkSubpassDependency {
    uint32_t srcSubpass = 0;
    uint32_t dstSubpass = 0;
    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;
    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = 0;
    VkFlags dependencyFlags = 0;
};

struct VkRenderPassCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    uint32_t attachmentCount = 0;
    const VkAttachmentDescription* pAttachments = nullptr;
    uint32_t subpassCount = 0;
    const VkSubpassDescription* pSubpasses = nullptr;
    uint32_t dependencyCount = 0;
    const VkSubpassDependency* pDependencies = nullptr;
};

struct VkPipelineShaderStageCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
    VkShaderModule module = 0;
    const char* pName = "main";
    const void* pSpecializationInfo = nullptr;
};

struct VkVertexInputBindingDescription {
    uint32_t binding = 0;
    uint32_t stride = 0;
    VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
};

struct VkVertexInputAttributeDescription {
    uint32_t location = 0;
    uint32_t binding = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t offset = 0;
};

struct VkPipelineVertexInputStateCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    uint32_t vertexBindingDescriptionCount = 0;
    const VkVertexInputBindingDescription* pVertexBindingDescriptions = nullptr;
    uint32_t vertexAttributeDescriptionCount = 0;
    const VkVertexInputAttributeDescription* pVertexAttributeDescriptions = nullptr;
};

struct VkPipelineInputAssemblyStateCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32 primitiveRestartEnable = 0;
};

struct VkViewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct VkPipelineViewportStateCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    uint32_t viewportCount = 1;
    const VkViewport* pViewports = nullptr;
    uint32_t scissorCount = 1;
    const VkRect2D* pScissors = nullptr;
};

struct VkPipelineRasterizationStateCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkBool32 depthClampEnable = 0;
    VkBool32 rasterizerDiscardEnable = 0;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlagBits cullMode = VK_CULL_MODE_NONE;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkBool32 depthBiasEnable = 0;
    float depthBiasConstantFactor = 0.0f;
    float depthBiasClamp = 0.0f;
    float depthBiasSlopeFactor = 0.0f;
    float lineWidth = 1.0f;
};

struct VkPipelineMultisampleStateCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkBool32 sampleShadingEnable = 0;
    float minSampleShading = 1.0f;
    const uint32_t* pSampleMask = nullptr;
    VkBool32 alphaToCoverageEnable = 0;
    VkBool32 alphaToOneEnable = 0;
};

struct VkStencilOpState {
    VkStencilOp failOp = VK_STENCIL_OP_KEEP;
    VkStencilOp passOp = VK_STENCIL_OP_KEEP;
    VkStencilOp depthFailOp = VK_STENCIL_OP_KEEP;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
    uint32_t compareMask = 0;
    uint32_t writeMask = 0;
    uint32_t reference = 0;
};

struct VkPipelineDepthStencilStateCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkBool32 depthTestEnable = 1;
    VkBool32 depthWriteEnable = 1;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkBool32 depthBoundsTestEnable = 0;
    VkBool32 stencilTestEnable = 0;
    VkStencilOpState front;
    VkStencilOpState back;
    float minDepthBounds = 0.0f;
    float maxDepthBounds = 1.0f;
};

struct VkPipelineColorBlendAttachmentState {
    VkBool32 blendEnable = 0;
    VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
    VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
    VkColorComponentFlags colorWriteMask = 0x0000000F;
};

struct VkPipelineColorBlendStateCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkBool32 logicOpEnable = 0;
    VkLogicOp logicOp = VK_LOGIC_OP_COPY;
    uint32_t attachmentCount = 1;
    const VkPipelineColorBlendAttachmentState* pAttachments = nullptr;
    float blendConstants[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct VkPushConstantRange {
    VkShaderStageFlags stageFlags = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
};

struct VkPipelineLayoutCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    uint32_t setLayoutCount = 0;
    const VkDescriptorSetLayout* pSetLayouts = nullptr;
    uint32_t pushConstantRangeCount = 0;
    const VkPushConstantRange* pPushConstantRanges = nullptr;
};

struct VkGraphicsPipelineCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    uint32_t stageCount = 0;
    const VkPipelineShaderStageCreateInfo* pStages = nullptr;
    const VkPipelineVertexInputStateCreateInfo* pVertexInputState = nullptr;
    const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState = nullptr;
    const void* pTessellationState = nullptr;
    const VkPipelineViewportStateCreateInfo* pViewportState = nullptr;
    const VkPipelineRasterizationStateCreateInfo* pRasterizationState = nullptr;
    const VkPipelineMultisampleStateCreateInfo* pMultisampleState = nullptr;
    const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState = nullptr;
    const VkPipelineColorBlendStateCreateInfo* pColorBlendState = nullptr;
    const void* pDynamicState = nullptr;
    VkPipelineLayout layout = 0;
    VkRenderPass renderPass = 0;
    uint32_t subpass = 0;
    VkPipeline basePipelineHandle = 0;
    int32_t basePipelineIndex = -1;
};

struct VkFramebufferCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkRenderPass renderPass = 0;
    uint32_t attachmentCount = 0;
    const VkImageView* pAttachments = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t layers = 1;
};

struct VkCommandPoolCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    uint32_t queueFamilyIndex = 0;
};

struct VkCommandBufferAllocateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    const void* pNext = nullptr;
    VkCommandPool commandPool = 0;
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    uint32_t commandBufferCount = 1;
};

struct VkCommandBufferBeginInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    const void* pInheritanceInfo = nullptr;
};

union VkClearColorValue {
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
};

struct VkClearDepthStencilValue {
    float depth = 1.0f;
    uint32_t stencil = 0;
};

union VkClearValue {
    VkClearColorValue color;
    VkClearDepthStencilValue depthStencil;
};

struct VkRenderPassBeginInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    const void* pNext = nullptr;
    VkRenderPass renderPass = 0;
    VkFramebuffer framebuffer = 0;
    VkRect2D renderArea;
    uint32_t clearValueCount = 0;
    const VkClearValue* pClearValues = nullptr;
};

struct VkSubmitInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    const void* pNext = nullptr;
    uint32_t waitSemaphoreCount = 0;
    const VkSemaphore* pWaitSemaphores = nullptr;
    const VkPipelineStageFlags* pWaitDstStageMask = nullptr;
    uint32_t commandBufferCount = 0;
    const VkCommandBuffer* pCommandBuffers = nullptr;
    uint32_t signalSemaphoreCount = 0;
    const VkSemaphore* pSignalSemaphores = nullptr;
};

struct VkPresentInfoKHR {
    VkStructureType sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    const void* pNext = nullptr;
    uint32_t waitSemaphoreCount = 0;
    const VkSemaphore* pWaitSemaphores = nullptr;
    uint32_t swapchainCount = 1;
    const VkSwapchainKHR* pSwapchains = nullptr;
    const uint32_t* pImageIndices = nullptr;
    VkResult* pResults = nullptr;
};

struct VkSemaphoreCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
};

struct VkFenceCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
};

struct VkBufferCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t queueFamilyIndexCount = 0;
    const uint32_t* pQueueFamilyIndices = nullptr;
};

struct VkMemoryRequirements {
    VkDeviceSize size = 0;
    VkDeviceSize alignment = 0;
    uint32_t memoryTypeBits = 0;
};

struct VkMemoryAllocateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    const void* pNext = nullptr;
    VkDeviceSize allocationSize = 0;
    uint32_t memoryTypeIndex = 0;
};

struct VkImageCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t queueFamilyIndexCount = 0;
    const uint32_t* pQueueFamilyIndices = nullptr;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct VkSamplerCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    float mipLodBias = 0.0f;
    VkBool32 anisotropyEnable = 0;
    float maxAnisotropy = 1.0f;
    VkBool32 compareEnable = 0;
    VkCompareOp compareOp = VK_COMPARE_OP_NEVER;
    float minLod = 0.0f;
    float maxLod = 0.0f;
    uint32_t borderColor = 0;
    VkBool32 unnormalizedCoordinates = 0;
};

struct VkDescriptorSetLayoutBinding {
    uint32_t binding = 0;
    VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    uint32_t descriptorCount = 1;
    VkShaderStageFlags stageFlags = 0;
    const VkSampler* pImmutableSamplers = nullptr;
};

struct VkDescriptorSetLayoutCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    uint32_t bindingCount = 0;
    const VkDescriptorSetLayoutBinding* pBindings = nullptr;
};

struct VkDescriptorPoolSize {
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    uint32_t descriptorCount = 0;
};

struct VkDescriptorPoolCreateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    const void* pNext = nullptr;
    VkFlags flags = 0;
    uint32_t maxSets = 0;
    uint32_t poolSizeCount = 0;
    const VkDescriptorPoolSize* pPoolSizes = nullptr;
};

struct VkDescriptorSetAllocateInfo {
    VkStructureType sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    const void* pNext = nullptr;
    VkDescriptorPool descriptorPool = 0;
    uint32_t descriptorSetCount = 0;
    const VkDescriptorSetLayout* pSetLayouts = nullptr;
};

struct VkDescriptorImageInfo {
    VkSampler sampler = 0;
    VkImageView imageView = 0;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
};

struct VkWriteDescriptorSet {
    VkStructureType sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    const void* pNext = nullptr;
    VkDescriptorSet dstSet = 0;
    uint32_t dstBinding = 0;
    uint32_t dstArrayElement = 0;
    uint32_t descriptorCount = 0;
    VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const VkDescriptorImageInfo* pImageInfo = nullptr;
    const void* pBufferInfo = nullptr;
    const void* pTexelBufferView = nullptr;
};

struct VkImageSubresourceLayers {
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t mipLevel = 0;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
};

struct VkBufferImageCopy {
    VkDeviceSize bufferOffset = 0;
    uint32_t bufferRowLength = 0;
    uint32_t bufferImageHeight = 0;
    VkImageSubresourceLayers imageSubresource;
    VkOffset3D imageOffset;
    VkExtent3D imageExtent;
};

struct VkImageMemoryBarrier {
    VkStructureType sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    const void* pNext = nullptr;
    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = 0;
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VkImage image = 0;
    VkImageSubresourceRange subresourceRange;
};

// Function pointers loaded from vulkan-1.dll on demand.
using PFN_vkCreateWin32SurfaceKHR = VkResult (*)(VkInstance, const VkWin32SurfaceCreateInfoKHR*, const void*, VkSurfaceKHR*);
using PFN_vkDestroySurfaceKHR = void (*)(VkInstance, VkSurfaceKHR, const void*);
using PFN_vkGetPhysicalDeviceSurfaceSupportKHR = VkResult (*)(VkPhysicalDevice, uint32_t, VkSurfaceKHR, VkBool32*);
using PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR*);
using PFN_vkGetPhysicalDeviceSurfaceFormatsKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, VkSurfaceFormatKHR*);
using PFN_vkGetPhysicalDeviceSurfacePresentModesKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, VkPresentModeKHR*);
using PFN_vkCreateSwapchainKHR = VkResult (*)(VkDevice, const VkSwapchainCreateInfoKHR*, const void*, VkSwapchainKHR*);
using PFN_vkDestroySwapchainKHR = void (*)(VkDevice, VkSwapchainKHR, const void*);
using PFN_vkGetSwapchainImagesKHR = VkResult (*)(VkDevice, VkSwapchainKHR, uint32_t*, VkImage*);
using PFN_vkAcquireNextImageKHR = VkResult (*)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*);
using PFN_vkQueuePresentKHR = VkResult (*)(VkQueue, const VkPresentInfoKHR*);
using PFN_vkCreateImageView = VkResult (*)(VkDevice, const VkImageViewCreateInfo*, const void*, VkImageView*);
using PFN_vkDestroyImageView = void (*)(VkDevice, VkImageView, const void*);
using PFN_vkCreateRenderPass = VkResult (*)(VkDevice, const VkRenderPassCreateInfo*, const void*, VkRenderPass*);
using PFN_vkDestroyRenderPass = void (*)(VkDevice, VkRenderPass, const void*);
using PFN_vkCreateFramebuffer = VkResult (*)(VkDevice, const VkFramebufferCreateInfo*, const void*, VkFramebuffer*);
using PFN_vkDestroyFramebuffer = void (*)(VkDevice, VkFramebuffer, const void*);
using PFN_vkCreatePipelineLayout = VkResult (*)(VkDevice, const VkPipelineLayoutCreateInfo*, const void*, VkPipelineLayout*);
using PFN_vkDestroyPipelineLayout = void (*)(VkDevice, VkPipelineLayout, const void*);
using PFN_vkCreateGraphicsPipelines = VkResult (*)(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo*, const void*, VkPipeline*);
using PFN_vkDestroyPipeline = void (*)(VkDevice, VkPipeline, const void*);
using PFN_vkCreateCommandPool = VkResult (*)(VkDevice, const VkCommandPoolCreateInfo*, const void*, VkCommandPool*);
using PFN_vkDestroyCommandPool = void (*)(VkDevice, VkCommandPool, const void*);
using PFN_vkAllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
using PFN_vkFreeCommandBuffers = void (*)(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*);
using PFN_vkResetCommandBuffer = VkResult (*)(VkCommandBuffer, VkFlags);
using PFN_vkBeginCommandBuffer = VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
using PFN_vkEndCommandBuffer = VkResult (*)(VkCommandBuffer);
using PFN_vkCmdBeginRenderPass = void (*)(VkCommandBuffer, const VkRenderPassBeginInfo*, uint32_t);
using PFN_vkCmdEndRenderPass = void (*)(VkCommandBuffer);
using PFN_vkCmdBindPipeline = void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
using PFN_vkCmdBindVertexBuffers = void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
using PFN_vkCmdBindIndexBuffer = void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType);
using PFN_vkCmdSetViewport = void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkViewport*);
using PFN_vkCmdSetScissor = void (*)(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*);
using PFN_vkCmdPushConstants = void (*)(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*);
using PFN_vkCmdDrawIndexed = void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
using PFN_vkQueueSubmit = VkResult (*)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
using PFN_vkQueueWaitIdle = VkResult (*)(VkQueue);
using PFN_vkDeviceWaitIdle = VkResult (*)(VkDevice);
using PFN_vkCreateSemaphore = VkResult (*)(VkDevice, const VkSemaphoreCreateInfo*, const void*, VkSemaphore*);
using PFN_vkDestroySemaphore = void (*)(VkDevice, VkSemaphore, const void*);
using PFN_vkCreateFence = VkResult (*)(VkDevice, const VkFenceCreateInfo*, const void*, VkFence*);
using PFN_vkDestroyFence = void (*)(VkDevice, VkFence, const void*);
using PFN_vkWaitForFences = VkResult (*)(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t);
using PFN_vkResetFences = VkResult (*)(VkDevice, uint32_t, const VkFence*);
using PFN_vkCreateBuffer = VkResult (*)(VkDevice, const VkBufferCreateInfo*, const void*, VkBuffer*);
using PFN_vkDestroyBuffer = void (*)(VkDevice, VkBuffer, const void*);
using PFN_vkGetBufferMemoryRequirements = void (*)(VkDevice, VkBuffer, VkMemoryRequirements*);
using PFN_vkAllocateMemory = VkResult (*)(VkDevice, const VkMemoryAllocateInfo*, const void*, VkDeviceMemory*);
using PFN_vkFreeMemory = void (*)(VkDevice, VkDeviceMemory, const void*);
using PFN_vkBindBufferMemory = VkResult (*)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
using PFN_vkMapMemory = VkResult (*)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkFlags, void**);
using PFN_vkUnmapMemory = void (*)(VkDevice, VkDeviceMemory);
using PFN_vkCreateImage = VkResult (*)(VkDevice, const VkImageCreateInfo*, const void*, VkImage*);
using PFN_vkDestroyImage = void (*)(VkDevice, VkImage, const void*);
using PFN_vkGetImageMemoryRequirements = void (*)(VkDevice, VkImage, VkMemoryRequirements*);
using PFN_vkBindImageMemory = VkResult (*)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
using PFN_vkCreateSampler = VkResult (*)(VkDevice, const VkSamplerCreateInfo*, const void*, VkSampler*);
using PFN_vkDestroySampler = void (*)(VkDevice, VkSampler, const void*);
using PFN_vkCreateDescriptorSetLayout = VkResult (*)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const void*, VkDescriptorSetLayout*);
using PFN_vkDestroyDescriptorSetLayout = void (*)(VkDevice, VkDescriptorSetLayout, const void*);
using PFN_vkCreateDescriptorPool = VkResult (*)(VkDevice, const VkDescriptorPoolCreateInfo*, const void*, VkDescriptorPool*);
using PFN_vkDestroyDescriptorPool = void (*)(VkDevice, VkDescriptorPool, const void*);
using PFN_vkAllocateDescriptorSets = VkResult (*)(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
using PFN_vkUpdateDescriptorSets = void (*)(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const void*);
using PFN_vkCmdBindDescriptorSets = void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
using PFN_vkCmdPipelineBarrier = void (*)(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkFlags, uint32_t, const void*, uint32_t, const void*, uint32_t, const VkImageMemoryBarrier*);
using PFN_vkCmdCopyBufferToImage = void (*)(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*);
using PFN_vkCmdCopyImageToBuffer = void (*)(VkCommandBuffer, VkImage, VkImageLayout, VkBuffer, uint32_t, const VkBufferImageCopy*);

} // namespace AstroPort
