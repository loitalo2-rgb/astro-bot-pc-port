// ==============================================================================
// test_pipeline.cpp — Diagnóstico Passo a Passo com Flush Imediato
// ==============================================================================

#include "config/config_manager.h"
#include "kernel/kernel_emulator.h"
#include "gpu/vulkan_loader.h"
#include "gpu/vulkan_backend.h"
#include "gpu/shader_recompiler.h"
#include "input/input_manager.h"
#include "core/eboot_loader.h"
#include "core/nid_resolver.h"
#include "patches/patch_manager.h"
#include <iostream>
#include <filesystem>

#define LOG(msg) do { std::cout << msg << std::endl; } while(0)

using namespace AstroPort;

int main() {
    LOG("================================================================");
    LOG("   ASTRO BOT PC — Pipeline Diagnostic & Hardware Test");
    LOG("================================================================\n");

    // 1. Teste de Configuração
    LOG("[Passo 1/5] Carregando Settings...");
    ConfigManager::Get().Load("config/settings.ini");
    const auto& cfg = ConfigManager::Get().GetConfig();
    auto res = ConfigManager::Get().GetActiveResolutionProfile();
    LOG("  -> Resolucao: " + std::string(res.nativeTag) + " (" + std::to_string(res.width) + "x" + std::to_string(res.height) + ")");

    // 2. Teste de Kernel Emulator
    LOG("\n[Passo 2/5] Testando KernelEmulator & Memoria Direta...");
    KernelEmulator::Get().Initialize();
    void* vramTest = VulkanBackend::Get().AllocateGarlicVram(16 * 1024 * 1024);
    void* onionTest = VulkanBackend::Get().AllocateOnionShared(8 * 1024 * 1024);
    LOG(std::string("  -> GARLIC (16MB): ") + (vramTest ? "OK" : "FALHA"));
    LOG(std::string("  -> ONION (8MB): ") + (onionTest ? "OK" : "FALHA"));

    // 3. Teste de Vulkan 1.3 REAL na GPU AMD Radeon
    LOG("\n[Passo 3/5] Testando Vulkan 1.3...");
    bool vkOk = VulkanBackend::Get().Initialize(nullptr, res.width, res.height, cfg.vsync);
    if (vkOk && VulkanBackend::Get().IsVulkanReady()) {
        const auto& gpu = VulkanBackend::Get().GetSelectedGpu();
        LOG("  -> GPU: " + gpu.deviceName);
        LOG("  -> VRAM: " + std::to_string(gpu.dedicatedVramMb) + " MB");

        LOG("  -> Testando geracao SPIR-V...");
        uint8_t dummyRdna2[] = {
            0x00, 0x00, 0x80, 0xBE,
            0x00, 0x00, 0x00, 0x02,
            0x00, 0x00, 0x00, 0xC4,
            0x00, 0x00, 0x81, 0xBF
        };
        auto compiled = ShaderRecompiler::Get().Recompile(dummyRdna2, sizeof(dummyRdna2), ShaderStage::Pixel);
        LOG(std::string("  -> Recompilacao SPIR-V: ") + (compiled.success ? "OK" : "FALHA"));

        if (compiled.success) {
            uint64_t sm = VulkanBackend::Get().CreateShaderModuleFromSpirv(compiled.spirvCode);
            LOG(std::string("  -> VkShaderModule na GPU AMD: ") + (sm ? "OK" : "FALHA"));
            if (sm) VulkanBackend::Get().DestroyShaderModule(sm);
        }
    } else {
        LOG("  -> Vulkan em modo emulado.");
    }

    // 4. Teste de Controles Xbox
    LOG("\n[Passo 4/5] Testando Sistema de Entrada Xbox/XInput...");
    InputConfig inputCfg{};
    inputCfg.stickDeadZoneInner = cfg.xboxDeadZoneInner;
    inputCfg.stickDeadZoneOuter = cfg.xboxDeadZoneOuter;
    inputCfg.triggerThreshold   = cfg.xboxTriggerThreshold;
    inputCfg.rumbleIntensity    = cfg.xboxRumbleIntensity;
    inputCfg.invertRightY       = cfg.xboxInvertRightY;
    InputManager::Get().SetInputConfig(inputCfg);
    InputManager::Get().Initialize(cfg.enableDualSense, cfg.enableXInput);
    InputManager::Get().Update();
    LOG("  -> Controles Xbox conectados: " + std::to_string(InputManager::Get().GetConnectedCount()));
    if (InputManager::Get().GetConnectedCount() > 0) {
        InputManager::Get().PrintState(0);
    }

    // 5. Teste de NIDs, Executável ELF e Patches
    LOG("\n[Passo 5/6] Testando NID Resolver e Importacao de link.json...");
    NidResolver::Get().BuildTable();
    const std::string linkJsonPath = "D:/Jogos ps4/issn-windows/issn-windows/out/link.json";
    if (std::filesystem::exists(linkJsonPath)) {
        NidResolver::Get().LoadFromJson(linkJsonPath);
    }

    LOG("\n[Passo 6/6] Testando Carregamento de Executavel e Patches...");
    const std::string execCandidates[] = {
        "D:/Jogos ps4/issn-windows/issn-windows/out/eboot.elf",
        "D:\\Games\\abaixar\\jogo\\PPSA21567\\eboot.bin",
        "eboot.bin"
    };

    std::string chosenExec;
    for (const auto& path : execCandidates) {
        if (std::filesystem::exists(path)) {
            chosenExec = path;
            break;
        }
    }

    if (!chosenExec.empty()) {
        LOG("  -> Carregando: " + chosenExec);
        bool loadOk = EbootLoader::Get().Load(chosenExec);
        LOG(std::string("  -> Validacao estrutural/relocacoes do executavel: ") +
            (loadOk ? "OK" : "FALHA"));
        if (loadOk) {
            bool patchOk = EbootLoader::Get().ApplyPatches();
            LOG(std::string("  -> Injeção de Patches (XML + ISA rewrite): ") + (patchOk ? "OK" : "FALHA"));
            LOG("  -> Total de patches ativos: " + std::to_string(PatchManager::Get().GetAppliedPatches().size()));
            LOG(std::string("  -> Execucao do jogo: ") +
                (EbootLoader::Get().Launch() ? "OK" : "NAO IMPLEMENTADA"));
        } else {
            LOG("  -> Jogo nao executado; o carregador recusou a imagem.");
        }
    } else {
        LOG("  -> Nenhum executavel encontrado.");
    }

    // 6. Conclusão
    LOG("\n[Finalizacao] Liberando recursos...");
    VulkanBackend::Get().Shutdown();
    KernelEmulator::Get().Shutdown();

    LOG("\n================================================================");
    LOG("   Diagnosticos concluidos; execucao completa do jogo nao foi validada.");
    LOG("================================================================");
    return 0;
}
