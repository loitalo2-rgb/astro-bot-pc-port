#include "gpu/upscaler_manager.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== Teste de Integracao: AMD FSR 4.1, FSR 3.1 & Intel XeSS ===" << std::endl;

    AstroPort::UpscalerManager manager;

    // Teste 1: AMD FSR 4.1 em 1440p Qualidade
    {
        AstroPort::UpscalerConfig cfg;
        cfg.backend = AstroPort::UpscalerBackend::AMD_FSR_4_1;
        cfg.quality = AstroPort::UpscalerQuality::Quality; // 1.5x
        cfg.displayWidth = 2560;
        cfg.displayHeight = 1440;
        cfg.sharpness = 0.85f;
        cfg.frameGeneration = true;
        cfg.shaderPath = "config/shaders/fsr4_shaders";

        bool ok = manager.Initialize(cfg);
        assert(ok);
        assert(manager.IsActive());
        assert(manager.GetConfig().renderWidth == 1706 || manager.GetConfig().renderWidth == 1708);
        assert(manager.GetConfig().renderHeight == 960);
        std::cout << "[OK] Teste 1: FSR 4.1 inicializado (Render: " 
                  << manager.GetConfig().renderWidth << "x" << manager.GetConfig().renderHeight 
                  << " -> 2560x1440, Shaders: " << manager.GetLoadedPassCount() << ")" << std::endl;
    }

    // Teste 2: AMD FSR 3.1 em 4K Desempenho (2.0x)
    {
        AstroPort::UpscalerConfig cfg;
        cfg.backend = AstroPort::UpscalerBackend::AMD_FSR_3_1;
        cfg.quality = AstroPort::UpscalerQuality::Performance; // 2.0x
        cfg.displayWidth = 3840;
        cfg.displayHeight = 2160;
        cfg.sharpness = 0.75f;
        cfg.frameGeneration = true;

        bool ok = manager.Initialize(cfg);
        assert(ok);
        assert(manager.GetConfig().renderWidth == 1920);
        assert(manager.GetConfig().renderHeight == 1080);
        std::cout << "[OK] Teste 2: FSR 3.1 inicializado (Render: 1920x1080 -> 3840x2160 4K)" << std::endl;
    }

    // Teste 3: Intel XeSS 1.3+ em 1440p Equilibrado (1.7x)
    {
        AstroPort::UpscalerConfig cfg;
        cfg.backend = AstroPort::UpscalerBackend::Intel_XeSS;
        cfg.quality = AstroPort::UpscalerQuality::Balanced;
        cfg.displayWidth = 2560;
        cfg.displayHeight = 1440;

        bool ok = manager.Initialize(cfg);
        assert(ok);
        assert(manager.GetLoadedPassCount() > 0);
        std::cout << "[OK] Teste 3: Intel XeSS inicializado (Render: "
                  << manager.GetConfig().renderWidth << "x" << manager.GetConfig().renderHeight << ")" << std::endl;
    }

    // Teste 4: Sequência de Jitter Subpixel Halton (2,3)
    {
        for (uint32_t f = 0; f < 8; ++f) {
            auto j = manager.GetJitterOffset(f);
            assert(j.x >= -0.5f && j.x <= 0.5f);
            assert(j.y >= -0.5f && j.y <= 0.5f);
        }
        std::cout << "[OK] Teste 4: Amostragem de Jitter Temporal Halton (2,3) validada" << std::endl;
    }

    std::cout << ">>> TODOS OS TESTES DE UPSCALER PASSARAM COM SUCESSO! <<<" << std::endl;
    return 0;
}
