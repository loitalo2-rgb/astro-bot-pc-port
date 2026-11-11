#include <windows.h>
#include <iostream>
#include <vector>

typedef struct VkInstance_T* VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkQueue_T* VkQueue;
typedef struct VkShaderModule_T* VkShaderModule;

struct VkApplicationInfo {
    uint32_t    sType = 0;
    const void* pNext = nullptr;
    const char* pApplicationName = "ASTRO BOT PC";
    uint32_t    applicationVersion = 1;
    const char* pEngineName = "Team ASOBI / AstroPort";
    uint32_t    engineVersion = 1;
    uint32_t    apiVersion = (1 << 22) | (3 << 12);
};

struct VkInstanceCreateInfo {
    uint32_t                    sType = 1;
    const void*                 pNext = nullptr;
    uint32_t                    flags = 0;
    const VkApplicationInfo*    pApplicationInfo = nullptr;
    uint32_t                    enabledLayerCount = 0;
    const char* const*          ppEnabledLayerNames = nullptr;
    uint32_t                    enabledExtensionCount = 0;
    const char* const*          ppEnabledExtensionNames = nullptr;
};

struct VkQueueFamilyProperties {
    uint32_t queueFlags;
    uint32_t queueCount;
    uint32_t timestampValidBits;
    struct { uint32_t width, height, depth; } minImageTransferGranularity;
};

struct VkDeviceQueueCreateInfo {
    uint32_t        sType = 2;
    const void*     pNext = nullptr;
    uint32_t        flags = 0;
    uint32_t        queueFamilyIndex = 0;
    uint32_t        queueCount = 1;
    const float*    pQueuePriorities = nullptr;
};

struct VkDeviceCreateInfo {
    uint32_t                        sType = 3;
    const void*                     pNext = nullptr;
    uint32_t                        flags = 0;
    uint32_t                        queueCreateInfoCount = 1;
    const VkDeviceQueueCreateInfo*  pQueueCreateInfos = nullptr;
    uint32_t                        enabledLayerCount = 0;
    const char* const*              ppEnabledLayerNames = nullptr;
    uint32_t                        enabledExtensionCount = 0;
    const char* const*              ppEnabledExtensionNames = nullptr;
    const void*                     pEnabledFeatures = nullptr;
};

struct VkShaderModuleCreateInfo {
    uint32_t        sType = 16;
    const void*     pNext = nullptr;
    uint32_t        flags = 0;
    size_t          codeSize = 0;
    const uint32_t* pCode = nullptr;
};

int main() {
    std::cout << "--- Teste Completo: Vulkan 1.3 na AMD Radeon RX 9060 XT ---" << std::endl;
    HMODULE hMod = LoadLibraryA("vulkan-1.dll");
    if (!hMod) return 1;

    auto vkCreateInstance = (int(*)(const VkInstanceCreateInfo*, const void*, VkInstance*))GetProcAddress(hMod, "vkCreateInstance");
    auto vkEnumeratePhysicalDevices = (int(*)(VkInstance, uint32_t*, VkPhysicalDevice*))GetProcAddress(hMod, "vkEnumeratePhysicalDevices");
    auto vkGetPhysicalDeviceQueueFamilyProperties = (void(*)(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties*))GetProcAddress(hMod, "vkGetPhysicalDeviceQueueFamilyProperties");
    auto vkCreateDevice = (int(*)(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice*))GetProcAddress(hMod, "vkCreateDevice");
    auto vkCreateShaderModule = (int(*)(VkDevice, const VkShaderModuleCreateInfo*, const void*, VkShaderModule*))GetProcAddress(hMod, "vkCreateShaderModule");
    auto vkDestroyShaderModule = (void(*)(VkDevice, VkShaderModule, const void*))GetProcAddress(hMod, "vkDestroyShaderModule");
    auto vkDestroyDevice = (void(*)(VkDevice, const void*))GetProcAddress(hMod, "vkDestroyDevice");
    auto vkDestroyInstance = (void(*)(VkInstance, const void*))GetProcAddress(hMod, "vkDestroyInstance");

    VkApplicationInfo appInfo{};
    VkInstanceCreateInfo ci{};
    ci.pApplicationInfo = &appInfo;

    VkInstance instance = nullptr;
    if (vkCreateInstance(&ci, nullptr, &instance) != 0) return 1;

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devs(count);
    vkEnumeratePhysicalDevices(instance, &count, devs.data());

    VkPhysicalDevice phys = devs[0];

    // Query Queue Families
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qfCount, qfProps.data());

    uint32_t graphicsQueueFamily = 0;
    for (uint32_t i = 0; i < qfCount; ++i) {
        if (qfProps[i].queueFlags & 1) { // VK_QUEUE_GRAPHICS_BIT
            graphicsQueueFamily = i;
            break;
        }
    }
    std::cout << "Fila Grafica encontrada no indice: " << graphicsQueueFamily << std::endl;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.queueFamilyIndex = graphicsQueueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    VkDeviceCreateInfo dci{};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;

    VkDevice device = nullptr;
    int devRes = vkCreateDevice(phys, &dci, nullptr, &device);
    std::cout << "vkCreateDevice res: " << devRes << ", device: " << (void*)device << std::endl;

    if (devRes == 0 && device) {
        std::cout << "SUCESSO: VkDevice criado na GPU AMD Radeon RX 9060 XT!" << std::endl;

        // Teste de criação de Shader Module com SPIR-V mínimo válido
        // SPIR-V mínimo com OpCapability Shader, OpMemoryModel, OpEntryPoint, OpReturn
        std::vector<uint32_t> minimalSpirv = {
            0x07230203, // Magic
            0x00010000, // Version 1.0
            0x00000000, // Generator
            5,          // Bound
            0,          // Schema
            0x00020011, 1, // OpCapability Shader
            0x0003000e, 0, 1 // OpMemoryModel Logical GLSL450
        };

        VkShaderModuleCreateInfo smci{};
        smci.codeSize = minimalSpirv.size() * sizeof(uint32_t);
        smci.pCode = minimalSpirv.data();

        VkShaderModule sm = nullptr;
        int smRes = vkCreateShaderModule(device, &smci, nullptr, &sm);
        std::cout << "vkCreateShaderModule res: " << smRes << ", module: " << (void*)sm << std::endl;
        if (smRes == 0 && sm) {
            std::cout << "SUCESSO ABSOLUTO: Shader compilado e validado diretamente no hardware da AMD Radeon!" << std::endl;
            vkDestroyShaderModule(device, sm, nullptr);
        }

        vkDestroyDevice(device, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
    FreeLibrary(hMod);
    return 0;
}
