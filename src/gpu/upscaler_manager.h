#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace AstroPort {

enum class UpscalerBackend {
    Native = 0,
    AMD_FSR_3_1 = 1,
    AMD_FSR_4_1 = 2,
    Intel_XeSS = 3
};

enum class UpscalerQuality {
    NativeAA = 0,
    UltraQuality = 1,
    Quality = 2,
    Balanced = 3,
    Performance = 4,
    UltraPerformance = 5
};

struct UpscalerConfig {
    UpscalerBackend backend = UpscalerBackend::AMD_FSR_4_1;
    UpscalerQuality quality = UpscalerQuality::Quality;
    uint32_t renderWidth = 1920;
    uint32_t renderHeight = 1080;
    uint32_t displayWidth = 2560;
    uint32_t displayHeight = 1440;
    float sharpness = 0.8f;              // 0.0f a 1.0f (RCAS sharpening)
    bool frameGeneration = false;        // FSR 3.1 / 4.1 Frame Interpolation
    bool hdrEnabled = true;
    std::string shaderPath;
};

struct JitterOffset {
    float x = 0.0f;
    float y = 0.0f;
};

class UpscalerManager {
public:
    UpscalerManager();
    ~UpscalerManager() = default;

    bool Initialize(const UpscalerConfig& config);
    void UpdateRenderResolution(uint32_t displayW, uint32_t displayH);
    
    // Calcula o deslocamento de subpixel (Halton 2,3) para Anti-Aliasing Temporal e Upscaling
    JitterOffset GetJitterOffset(uint32_t frameIndex);
    
    // Fator de escala baseado no modo de qualidade (ex: Quality = 1.5x, Performance = 2.0x)
    static float GetScaleFactor(UpscalerQuality quality);
    static const char* GetBackendName(UpscalerBackend backend);
    static const char* GetQualityName(UpscalerQuality quality);

    const UpscalerConfig& GetConfig() const { return m_config; }
    bool IsActive() const { return m_config.backend != UpscalerBackend::Native; }
    size_t GetLoadedPassCount() const { return m_loadedPassCount; }

private:
    UpscalerConfig m_config;
    size_t m_loadedPassCount = 0;
    bool m_initialized = false;
    
    // Halton sequence generator
    static float Halton(uint32_t index, uint32_t base);
};

} // namespace AstroPort
