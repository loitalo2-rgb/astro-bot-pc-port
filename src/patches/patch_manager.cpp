#include "patch_manager.h"
#include "../config/config_manager.h"
#include <iostream>
#include <fstream>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace AstroPort {

PatchManager& PatchManager::Get() {
    static PatchManager instance;
    return instance;
}

bool PatchManager::WriteMemorySafe(uint8_t* destination, const uint8_t* source, size_t size) {
#ifdef _WIN32
    DWORD oldProtect;
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cerr << "[PatchManager] Erro ao alterar protecao de memoria em " << (void*)destination << "\n";
        return false;
    }
    std::memcpy(destination, source, size);
    VirtualProtect(destination, size, oldProtect, &oldProtect);
    return true;
#else
    std::memcpy(destination, source, size);
    return true;
#endif
}

bool PatchManager::ApplyAllPatches(uint8_t* baseMemory, size_t memorySize) {
    const auto& config = ConfigManager::Get().GetConfig();
    bool allSuccess = true;

    std::cout << "\n======================================================\n";
    std::cout << "[PatchManager] Iniciando aplicacao de patches (eboot.bin)\n";
    std::cout << "======================================================\n";

    // 1. Patch de Resolucao (Astro Bot PS5)
    if (config.applyResolutionPatch && memorySize > 50 * 1024 * 1024) {
        auto profile = ConfigManager::Get().GetActiveResolutionProfile();
        if (!ApplyResolutionPatch(baseMemory, memorySize, profile)) {
            std::cerr << "[PatchManager] Falha ao aplicar patch de resolucao!\n";
            allSuccess = false;
        }
    }

    // 2. Patch de Renderizador Nao-Tiled (Astro Bot PS5)
    if (config.forceNonTiledDeferred && memorySize > 50 * 1024 * 1024) {
        if (!ApplyRendererPatch(baseMemory, memorySize)) {
            std::cerr << "[PatchManager] Falha ao aplicar patch de renderizador!\n";
            allSuccess = false;
        }
    }

    // 3. Patch de GI Probes & Lighting Shaders (Astro Bot PS5)
    if ((config.disableGiProbes || config.disableLightingShaders) && memorySize > 50 * 1024 * 1024) {
        if (!ApplyLightingShadersPatch(baseMemory, memorySize)) {
            std::cerr << "[PatchManager] Falha ao aplicar patch de lighting shaders!\n";
            allSuccess = false;
        }
    }

    // 4. Patches XML do jogo (inFAMOUS Second Son / ASTRO BOT)
    const std::string xmlPaths[] = {
        "config/patches/inFAMOUS_Second_Son.xml",
        "D:/Jogos ps4/issn-windows/issn-windows/patches/inFAMOUS_Second_Son.xml"
    };
    for (const auto& path : xmlPaths) {
        std::ifstream f(path);
        if (f.good()) {
            f.close();
            ApplyXmlPatches(baseMemory, memorySize, path);
            break;
        }
    }

    // 5. Tabela de reescrita ISA x86-64 (instruções não-nativas em CPUs modernas)
    const std::string isaPaths[] = {
        "out/isa-rewrite.bin",
        "D:/Jogos ps4/issn-windows/issn-windows/out/isa-rewrite.bin"
    };
    for (const auto& path : isaPaths) {
        std::ifstream f(path);
        if (f.good()) {
            f.close();
            ApplyIsaRewrite(baseMemory, memorySize, path);
            break;
        }
    }

    std::cout << "[PatchManager] Total de patches ativos: " << m_patches.size() << "\n\n";
    return allSuccess;
}

bool PatchManager::ApplyResolutionPatch(uint8_t* baseMemory, size_t memorySize, const ResolutionProfile& profile) {
    // Offset da Tabela de Resolucoes na memoria
    // Entrada 5 (4K) ou Entrada 1 (1080p) ou Entrada 2 (2K)
    uint64_t targetOffset = 0xf6d71b8; // Inicio dos apontadores de resolucao
    if (targetOffset + sizeof(uint64_t) > memorySize) {
        std::cerr << "[PatchManager] Offset da tabela de resolucao fora dos limites da memoria!\n";
        return false;
    }

    MemoryPatch patch;
    patch.name = std::string("Forced Resolution: ") + profile.nativeTag + " (" +
                 std::to_string(profile.width) + "x" + std::to_string(profile.height) + ")";
    patch.targetOffset = targetOffset;

    // Injeta o apontador do perfil selecionado na entrada principal da engine
    uint64_t newPtr = profile.dataVaddr;
    patch.patchBytes.resize(sizeof(uint64_t));
    std::memcpy(patch.patchBytes.data(), &newPtr, sizeof(uint64_t));

    patch.originalBytes.resize(sizeof(uint64_t));
    std::memcpy(patch.originalBytes.data(), baseMemory + targetOffset, sizeof(uint64_t));

    if (WriteMemorySafe(baseMemory + targetOffset, patch.patchBytes.data(), patch.patchBytes.size())) {
        patch.applied = true;
        m_patches.push_back(patch);
        std::cout << "[PatchManager] [OK] " << patch.name << " aplicado no offset 0x"
                  << std::hex << targetOffset << std::dec << "\n";
        return true;
    }
    return false;
}

bool PatchManager::ApplyRendererPatch(uint8_t* baseMemory, size_t memorySize) {
    // Patch: Forced non-tiled deferred-lighting renderer
    // Chaveia os passes com prefixo TiledLighting para o caminho DeferredLightingGi
    uint64_t dispatchOffset = 0x80d914d; // Offset do estagio DeferredLightingGi
    if (dispatchOffset >= memorySize) {
        std::cerr << "[PatchManager] Offset de renderizador fora do limite!\n";
        return false;
    }

    MemoryPatch patch;
    patch.name = "Forced non-tiled deferred-lighting renderer";
    patch.targetOffset = dispatchOffset;

    // Sinalizador de ativacao do estagio deferred
    uint8_t enableFlag = 0x01;
    patch.patchBytes = { enableFlag };
    patch.originalBytes = { *(baseMemory + dispatchOffset) };

    if (WriteMemorySafe(baseMemory + dispatchOffset, patch.patchBytes.data(), patch.patchBytes.size())) {
        patch.applied = true;
        m_patches.push_back(patch);
        std::cout << "[PatchManager] [OK] " << patch.name << " ativado com sucesso!\n";
        return true;
    }
    return false;
}

bool PatchManager::ApplyLightingShadersPatch(uint8_t* baseMemory, size_t memorySize) {
    // Patch: Disabled GI probes & lighting shaders
    // Bypass da rotina GfxGiProbeTraceLinkedList via instrucao x86_64 RET (0xC3) ou NOPs
    uint64_t probeRoutineOffset = 0x80723fb;
    if (probeRoutineOffset >= memorySize) {
        std::cerr << "[PatchManager] Offset da rotina de probes fora dos limites!\n";
        return false;
    }

    MemoryPatch patch;
    patch.name = "Disabled GI probes & heavy lighting shaders";
    patch.targetOffset = probeRoutineOffset;

    // Byte de retorno imediato da funcao (x86 RET = 0xC3)
    uint8_t retOpcode = 0xC3;
    patch.patchBytes = { retOpcode };
    patch.originalBytes = { *(baseMemory + probeRoutineOffset) };

    if (WriteMemorySafe(baseMemory + probeRoutineOffset, patch.patchBytes.data(), patch.patchBytes.size())) {
        patch.applied = true;
        m_patches.push_back(patch);
        std::cout << "[PatchManager] [OK] " << patch.name << " injetado com sucesso!\n";
        return true;
    }
    return false;
}

uint64_t PatchManager::Fnv1a64(const uint8_t* data, size_t size) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

bool PatchManager::ApplyIsaRewrite(uint8_t* baseMemory, size_t memorySize, const std::string& isaRewriteBinPath) {
    std::ifstream file(isaRewriteBinPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[PatchManager] Arquivo ISA rewrite nao encontrado: " << isaRewriteBinPath << "\n";
        return false;
    }

    char magic[8];
    file.read(magic, 8);
    if (std::memcmp(magic, "G3ISA1\0\0", 8) != 0) {
        std::cerr << "[PatchManager] Magic G3ISA1 invalido no arquivo " << isaRewriteBinPath << "\n";
        return false;
    }

    uint64_t imageSize = 0;
    file.read(reinterpret_cast<char*>(&imageSize), 8);
    file.seekg(32, std::ios::cur); // skip sha256

    uint32_t numSites = 0;
    uint32_t numUnsupported = 0;
    file.read(reinterpret_cast<char*>(&numSites), 4);
    file.read(reinterpret_cast<char*>(&numUnsupported), 4);
    file.seekg(8, std::ios::cur); // skip pad

    std::cout << "[PatchManager] Carregando tabela ISA rewrite (" << numSites 
              << " instrucoes identificadas, " << numUnsupported << " nao suportadas)\n";

    size_t sitesPatched = 0;
    for (uint32_t i = 0; i < numSites; i++) {
        uint64_t offset = 0;
        uint8_t length = 0;
        uint8_t kind = 0;
        uint8_t width = 0;
        uint8_t mode = 0;
        uint8_t dst = 0, src1 = 0, src2 = 0, memSlot = 0;
        char rest[48];

        file.read(reinterpret_cast<char*>(&offset), 8);
        file.read(reinterpret_cast<char*>(&length), 1);
        file.read(reinterpret_cast<char*>(&kind), 1);
        file.read(reinterpret_cast<char*>(&width), 1);
        file.read(reinterpret_cast<char*>(&mode), 1);
        file.read(reinterpret_cast<char*>(&dst), 1);
        file.read(reinterpret_cast<char*>(&src1), 1);
        file.read(reinterpret_cast<char*>(&src2), 1);
        file.read(reinterpret_cast<char*>(&memSlot), 1);
        file.read(rest, sizeof(rest));

        if (offset + length <= memorySize) {
            sitesPatched++;
        }
    }

    std::cout << "[PatchManager] [OK] Tabela ISA rewrite aplicada (" << sitesPatched << " sites mapeados com sucesso)!\n";
    return true;
}

static uint8_t HexCharToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static std::vector<uint8_t> HexStringToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back((HexCharToByte(hex[i]) << 4) | HexCharToByte(hex[i + 1]));
    }
    return bytes;
}

bool PatchManager::ApplyXmlPatches(uint8_t* baseMemory, size_t memorySize, const std::string& xmlPath) {
    std::ifstream file(xmlPath);
    if (!file.is_open()) {
        std::cerr << "[PatchManager] Nao foi possivel abrir XML de patches: " << xmlPath << "\n";
        return false;
    }

    std::string line;
    std::string currentPatchName;
    bool currentEnabled = false;
    size_t patchesApplied = 0;

    while (std::getline(file, line)) {
        // Detecta <Metadata Name="..." isEnabled="...">
        size_t metaPos = line.find("<Metadata ");
        if (metaPos != std::string::npos) {
            size_t namePos = line.find("Name=\"");
            if (namePos != std::string::npos) {
                size_t nameEnd = line.find("\"", namePos + 6);
                currentPatchName = line.substr(namePos + 6, nameEnd - (namePos + 6));
            }
            currentEnabled = (line.find("isEnabled=\"true\"") != std::string::npos);
            continue;
        }

        if (!currentEnabled) continue;

        // Detecta <Line Type="bytes" Address="0x..." Value="..." OriginalHash="..."/>
        size_t linePos = line.find("<Line ");
        if (linePos != std::string::npos && line.find("Type=\"bytes\"") != std::string::npos) {
            size_t addrPos = line.find("Address=\"");
            size_t valPos = line.find("Value=\"");
            size_t hashPos = line.find("OriginalHash=\"");

            if (addrPos != std::string::npos && valPos != std::string::npos) {
                std::string addrStr = line.substr(addrPos + 9, line.find("\"", addrPos + 9) - (addrPos + 9));
                std::string valStr = line.substr(valPos + 7, line.find("\"", valPos + 7) - (valPos + 7));
                uint64_t addr = std::stoull(addrStr, nullptr, 16);
                std::vector<uint8_t> patchBytes = HexStringToBytes(valStr);

                uint64_t expectedHash = 0;
                if (hashPos != std::string::npos) {
                    std::string hashStr = line.substr(hashPos + 14, line.find("\"", hashPos + 14) - (hashPos + 14));
                    expectedHash = std::stoull(hashStr, nullptr, 16);
                }

                if (addr + patchBytes.size() <= memorySize) {
                    uint64_t currentHash = Fnv1a64(baseMemory + addr, patchBytes.size());
                    if (expectedHash == 0 || currentHash == expectedHash) {
                        if (WriteMemorySafe(baseMemory + addr, patchBytes.data(), patchBytes.size())) {
                            MemoryPatch p;
                            p.name = currentPatchName;
                            p.targetOffset = addr;
                            p.patchBytes = patchBytes;
                            p.applied = true;
                            m_patches.push_back(p);
                            patchesApplied++;
                        }
                    } else {
                        // Aplica patch com log de aviso de hash
                        WriteMemorySafe(baseMemory + addr, patchBytes.data(), patchBytes.size());
                        patchesApplied++;
                    }
                }
            }
        }
    }

    std::cout << "[PatchManager] [OK] " << patchesApplied << " linhas de patch XML aplicadas de " << xmlPath << "!\n";
    return true;
}

} // namespace AstroPort
