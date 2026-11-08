#pragma once

#include "../common/types.h"
#include <string>

namespace AstroPort {

struct AppConfig {
    // Performance
    bool fastLoad = false; // Se true, usa cache persistente e pula recompilação completa

    // Display
    ResolutionMode resolution = ResolutionMode::Res_1080p;
    uint32_t targetWidth = 1920;
    uint32_t targetHeight = 1080;
    std::string windowMode = "Borderless";
    bool vsync = true;
    uint32_t framerateLimit = 60;
    float internalRenderScale = 1.0f;

    // Patches
    bool forceNonTiledDeferred = true;
    bool disableGiProbes = true;
    bool disableLightingShaders = true;
    bool applyResolutionPatch = true;

    // Upscaling & FSR 4
    bool enableFsr4 = true;
    std::string fsr4Quality = "Balanced"; // Native, Quality, Balanced, UltraPerformance
    float rcasSharpening = 0.8f;

    // Graphics
    std::string rendererBackend = "Vulkan";
    uint32_t gpuIndex = 0;
    bool enableValidationLayers = false;

    // Audio
    std::string audioBackend = "WASAPI";
    uint32_t audioChannels = 2;
    uint32_t masterVolume = 100;

    // Input
    bool enableDualSense = true;
    bool enableXInput = true;
    float xboxDeadZoneInner = 0.12f;
    float xboxDeadZoneOuter = 0.95f;
    float xboxTriggerThreshold = 0.15f;
    uint32_t xboxRumbleIntensity = 100;
    bool xboxInvertRightY = false;

    // System
    std::string gameDataPath;
    std::string saveDataPath = "./savedata";
};

class ConfigManager {
public:
    static ConfigManager& Get();

    bool Load(const std::string& iniFilePath);
    const AppConfig& GetConfig() const { return m_config; }

    ResolutionProfile GetActiveResolutionProfile() const;

private:
    ConfigManager() = default;
    AppConfig m_config;

    std::string Trim(const std::string& str);
};

} // namespace AstroPort
