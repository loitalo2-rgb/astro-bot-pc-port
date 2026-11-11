#include "vulkan_loader.h"
#include <iostream>

namespace AstroPort {

VulkanLoader& VulkanLoader::Get() {
    static VulkanLoader instance;
    return instance;
}

bool VulkanLoader::Load() {
    if (m_available) return true;

#ifdef _WIN32
    m_hModule = LoadLibraryA("vulkan-1.dll");
    if (!m_hModule) {
        std::cerr << "[VulkanLoader] Erro: vulkan-1.dll nao encontrada no sistema!\n";
        return false;
    }

    auto loadProc = [this](const char* name) -> void* {
        return (void*)GetProcAddress(m_hModule, name);
    };

    vkCreateInstance = (PFN_vkCreateInstance)loadProc("vkCreateInstance");
    vkDestroyInstance = (PFN_vkDestroyInstance)loadProc("vkDestroyInstance");
    vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)loadProc("vkEnumeratePhysicalDevices");
    vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)loadProc("vkGetPhysicalDeviceProperties");
    vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)loadProc("vkGetPhysicalDeviceFeatures");
    vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)loadProc("vkGetPhysicalDeviceQueueFamilyProperties");
    vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)loadProc("vkGetPhysicalDeviceMemoryProperties");
    vkCreateDevice = (PFN_vkCreateDevice)loadProc("vkCreateDevice");
    vkDestroyDevice = (PFN_vkDestroyDevice)loadProc("vkDestroyDevice");
    vkGetDeviceQueue = (PFN_vkGetDeviceQueue)loadProc("vkGetDeviceQueue");
    vkCreateShaderModule = (PFN_vkCreateShaderModule)loadProc("vkCreateShaderModule");
    vkDestroyShaderModule = (PFN_vkDestroyShaderModule)loadProc("vkDestroyShaderModule");

    if (!vkCreateInstance || !vkEnumeratePhysicalDevices || !vkCreateDevice) {
        std::cerr << "[VulkanLoader] Falha ao carregar funcoes basicas de vulkan-1.dll!\n";
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
        return false;
    }

    m_available = true;
    std::cout << "[VulkanLoader] vulkan-1.dll carregada com sucesso a partir do driver da GPU!\n";
    return true;
#else
    return false;
#endif
}

void* VulkanLoader::GetSymbol(const char* name) const {
#if defined(_WIN32)
    if (!m_hModule || !name) return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(m_hModule, name));
#else
    (void)name;
    return nullptr;
#endif
}

void VulkanLoader::Unload() {
#ifdef _WIN32
    if (m_hModule) {
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
    }
#endif
    m_available = false;
}

} // namespace AstroPort
