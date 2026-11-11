#pragma once

#include "../common/types.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace AstroPort {

struct MemoryPatch {
    std::string name;
    uint64_t targetOffset;
    std::vector<uint8_t> originalBytes;
    std::vector<uint8_t> patchBytes;
    bool applied = false;
};

class PatchManager {
public:
    static PatchManager& Get();

    // Aplica todos os patches configurados sobre o buffer de memória mapeado
    bool ApplyAllPatches(uint8_t* baseMemory, size_t memorySize);

    // Patches Individuais
    bool ApplyResolutionPatch(uint8_t* baseMemory, size_t memorySize, const ResolutionProfile& profile);
    bool ApplyRendererPatch(uint8_t* baseMemory, size_t memorySize);
    bool ApplyLightingShadersPatch(uint8_t* baseMemory, size_t memorySize);

    // Patches Avançados (inFAMOUS / ISA rewrite)
    bool ApplyIsaRewrite(uint8_t* baseMemory, size_t memorySize, const std::string& isaRewriteBinPath);
    bool ApplyXmlPatches(uint8_t* baseMemory, size_t memorySize, const std::string& xmlPath);

    static uint64_t Fnv1a64(const uint8_t* data, size_t size);

    const std::vector<MemoryPatch>& GetAppliedPatches() const { return m_patches; }

private:
    PatchManager() = default;
    std::vector<MemoryPatch> m_patches;

    bool WriteMemorySafe(uint8_t* destination, const uint8_t* source, size_t size);
};

} // namespace AstroPort
