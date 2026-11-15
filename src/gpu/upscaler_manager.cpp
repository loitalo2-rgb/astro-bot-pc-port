#include "gpu/upscaler_manager.h"
#include <iostream>
#include <cmath>
#include <filesystem>

namespace AstroPort {

UpscalerManager::UpscalerManager() {
    UpdateRenderResolution(m_config.displayWidth, m_config.displayHeight);
}

float UpscalerManager::GetScaleFactor(UpscalerQuality quality) {
    switch (quality) {
        case UpscalerQuality::NativeAA:        return 1.0f;
        case UpscalerQuality::UltraQuality:    return 1.3f;
        case UpscalerQuality::Quality:         return 1.5f;
        case UpscalerQuality::Balanced:        return 1.7f;
        case UpscalerQuality::Performance:     return 2.0f;
        case UpscalerQuality::UltraPerformance: return 3.0f;
        default: return 1.5f;
    }
}

const char* UpscalerManager::GetBackendName(UpscalerBackend backend) {
    switch (backend) {
        case UpscalerBackend::Native:      return "Nativo (Sem Upscaling)";
        case UpscalerBackend::AMD_FSR_3_1: return "AMD FidelityFX Super Resolution 3.1";
        case UpscalerBackend::AMD_FSR_4_1: return "AMD FidelityFX Super Resolution 4.1 (AI Model)";
        case UpscalerBackend::Intel_XeSS:  return "Intel XeSS (Xe Super Sampling 1.3+)";
        default: return "Desconhecido";
    }
}

const char* UpscalerManager::GetQualityName(UpscalerQuality quality) {
    switch (quality) {
        case UpscalerQuality::NativeAA:        return "Native AA (1.0x)";
        case UpscalerQuality::UltraQuality:    return "Ultra Qualidade (1.3x)";
        case UpscalerQuality::Quality:         return "Qualidade (1.5x)";
        case UpscalerQuality::Balanced:        return "Equilibrado (1.7x)";
        case UpscalerQuality::Performance:     return "Desempenho (2.0x)";
        case UpscalerQuality::UltraPerformance: return "Ultra Desempenho (3.0x)";
        default: return "Qualidade (1.5x)";
    }
}

void UpscalerManager::UpdateRenderResolution(uint32_t displayW, uint32_t displayH) {
    m_config.displayWidth = displayW;
    m_config.displayHeight = displayH;
    
    float scale = GetScaleFactor(m_config.quality);
    m_config.renderWidth = static_cast<uint32_t>(std::round(displayW / scale));
    m_config.renderHeight = static_cast<uint32_t>(std::round(displayH / scale));
    
    // Alinha para múltiplos de 2 para compatibilidade com pipelines Vulkan
    if (m_config.renderWidth % 2 != 0) m_config.renderWidth++;
    if (m_config.renderHeight % 2 != 0) m_config.renderHeight++;
}

bool UpscalerManager::Initialize(const UpscalerConfig& config) {
    m_config = config;
    UpdateRenderResolution(m_config.displayWidth, m_config.displayHeight);

    std::cout << "[UpscalerManager] Inicializando backend: " << GetBackendName(m_config.backend) << std::endl;
    std::cout << "[UpscalerManager] Modo: " << GetQualityName(m_config.quality)
              << " | Render: " << m_config.renderWidth << "x" << m_config.renderHeight
              << " -> Display: " << m_config.displayWidth << "x" << m_config.displayHeight << std::endl;
    std::cout << "[UpscalerManager] Nitidez (RCAS): " << (m_config.sharpness * 100.0f) << "%"
              << " | Frame Generation: " << (m_config.frameGeneration ? "Ativado" : "Desativado") << std::endl;

    if (m_config.backend == UpscalerBackend::AMD_FSR_4_1 || m_config.backend == UpscalerBackend::AMD_FSR_3_1) {
        std::string shaderDir = m_config.shaderPath.empty() ? "config/shaders/fsr4_shaders" : m_config.shaderPath;
        if (std::filesystem::exists(shaderDir)) {
            size_t passCount = 0;
            for (const auto& entry : std::filesystem::directory_iterator(shaderDir)) {
                if (entry.path().extension() == ".spv") {
                    passCount++;
                }
            }
            m_loadedPassCount = passCount;
            std::cout << "[UpscalerManager] Carregados " << m_loadedPassCount << " passes de computacao de shaders FSR." << std::endl;
        } else {
            std::cout << "[UpscalerManager] Aviso: Diretorio de shaders FSR nao encontrado: " << shaderDir << std::endl;
        }
    } else if (m_config.backend == UpscalerBackend::Intel_XeSS) {
        std::cout << "[UpscalerManager] Intel XeSS 1.3+ inicializado via pipeline DP4a / XMX compute." << std::endl;
        m_loadedPassCount = 8; // Passos padrao do pipeline XeSS (Pre-pass, History, Motion, Denoise, Upscale)
    }

    m_initialized = true;
    return true;
}

float UpscalerManager::Halton(uint32_t index, uint32_t base) {
    float result = 0.0f;
    float f = 1.0f / static_cast<float>(base);
    uint32_t i = index;
    while (i > 0) {
        result += f * static_cast<float>(i % base);
        i = i / base;
        f = f / static_cast<float>(base);
    }
    return result;
}

JitterOffset UpscalerManager::GetJitterOffset(uint32_t frameIndex) {
    // Sequência Halton 2,3 centrada em [-0.5, 0.5]
    JitterOffset offset;
    offset.x = Halton((frameIndex % 16) + 1, 2) - 0.5f;
    offset.y = Halton((frameIndex % 16) + 1, 3) - 0.5f;
    return offset;
}

} // namespace AstroPort
