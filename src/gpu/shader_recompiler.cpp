#include "shader_recompiler.h"
#include "../config/config_manager.h"
#include <fstream>
#include <mutex>
#include <future>
#include <thread>
#include <filesystem>

#include <iostream>
#include <algorithm>
#include <cassert>
#include <cstring>

namespace AstroPort {

// ==============================================================================
// SPIR-V Magic, Version e Opcodes (subset suficiente para geração estrutural)
// Referência: https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html
// ==============================================================================
namespace Spv {
    constexpr uint32_t MagicNumber          = 0x07230203;
    constexpr uint32_t Version_1_5          = 0x00010500;
    constexpr uint32_t GeneratorMagic       = 0xAB0001;  // AstroPort Recompiler v1

    // Core opcodes
    constexpr uint32_t OpCapability         = 17;
    constexpr uint32_t OpExtInstImport      = 11;
    constexpr uint32_t OpMemoryModel        = 14;
    constexpr uint32_t OpEntryPoint         = 15;
    constexpr uint32_t OpExecutionMode      = 16;
    constexpr uint32_t OpName               = 5;
    constexpr uint32_t OpDecorate           = 71;
    constexpr uint32_t OpTypeVoid           = 19;
    constexpr uint32_t OpTypeBool           = 20;
    constexpr uint32_t OpTypeInt            = 21;
    constexpr uint32_t OpTypeFloat          = 22;
    constexpr uint32_t OpTypeVector         = 23;
    constexpr uint32_t OpTypeMatrix         = 24;
    constexpr uint32_t OpTypeImage          = 25;
    constexpr uint32_t OpTypeSampler        = 26;
    constexpr uint32_t OpTypeSampledImage   = 27;
    constexpr uint32_t OpTypeArray          = 28;
    constexpr uint32_t OpTypePointer        = 32;
    constexpr uint32_t OpTypeFunction       = 33;
    constexpr uint32_t OpConstant           = 43;
    constexpr uint32_t OpConstantComposite  = 44;
    constexpr uint32_t OpFunction           = 54;
    constexpr uint32_t OpFunctionEnd        = 56;
    constexpr uint32_t OpVariable           = 59;
    constexpr uint32_t OpLoad               = 61;
    constexpr uint32_t OpStore              = 62;
    constexpr uint32_t OpReturn             = 253;
    constexpr uint32_t OpLabel              = 248;

    // Capabilities
    constexpr uint32_t CapShader            = 1;
    constexpr uint32_t CapFloat16           = 9;

    // Addressing & Memory models
    constexpr uint32_t AddressingModelLogical   = 0;
    constexpr uint32_t MemoryModelGLSL450       = 1;

    // Storage classes
    constexpr uint32_t StorageInput             = 1;
    constexpr uint32_t StorageOutput            = 3;
    constexpr uint32_t StorageUniformConstant   = 0;
    constexpr uint32_t StorageFunction          = 7;

    // Execution models
    constexpr uint32_t ExecutionModelVertex     = 0;
    constexpr uint32_t ExecutionModelFragment   = 4;
    constexpr uint32_t ExecutionModelGLCompute  = 5;

    // Execution modes
    constexpr uint32_t ExecutionModeOriginUpperLeft = 7;

    // Decorations
    constexpr uint32_t DecorationLocation      = 30;
    constexpr uint32_t DecorationBinding       = 33;
    constexpr uint32_t DecorationBuiltIn       = 11;
}

// ==============================================================================
// RDNA2 Instruções (ISA subset relevante para análise)
// ==============================================================================
namespace Rdna2 {
    // SOP (Scalar ALU)
    constexpr uint32_t S_MOV_B32   = 0xBE800000;
    constexpr uint32_t S_MOV_B64   = 0xBE800001;
    constexpr uint32_t S_AND_B32   = 0x8F000000;
    constexpr uint32_t S_ADD_U32   = 0x80000000;

    // VOP (Vector ALU)
    constexpr uint32_t V_MOV_B32   = 0x7E000002;
    constexpr uint32_t V_ADD_F32   = 0x02000000;
    constexpr uint32_t V_MUL_F32   = 0x08000000;
    constexpr uint32_t V_MAD_F32   = 0xD2820000; // VOP3
    constexpr uint32_t V_FMA_F32   = 0xD2960000; // VOP3
    constexpr uint32_t V_CVT_F32_I32 = 0x7E00000B;

    // VMEM (Vector Memory)
    constexpr uint32_t TBUFFER_LOAD_FORMAT_XYZW = 0xE8C00000;
    constexpr uint32_t TBUFFER_STORE_FORMAT_XYZW = 0xEC000000;
    constexpr uint32_t IMAGE_SAMPLE             = 0xF0800000;
    constexpr uint32_t IMAGE_SAMPLE_L           = 0xF0880000;

    // SMEM (Scalar Memory)
    constexpr uint32_t S_LOAD_DWORD             = 0xC0020000;
    constexpr uint32_t S_LOAD_DWORDX2           = 0xC0060000;
    constexpr uint32_t S_LOAD_DWORDX4           = 0xC00A0000;

    // Controle de Fluxo
    constexpr uint32_t S_ENDPGM                 = 0xBF810000;
    constexpr uint32_t S_BRANCH                 = 0xBF820000;
    constexpr uint32_t S_CBRANCH_SCC0           = 0xBF840000;
    constexpr uint32_t V_CNDMASK_B32            = 0x00000000;

    // Export (saída do PS / VS)
    constexpr uint32_t EXP                      = 0xC4000000;
}

// ==============================================================================
// ShaderRecompiler — Implementação
// ==============================================================================

ShaderRecompiler& ShaderRecompiler::Get() {
    static ShaderRecompiler instance;
    return instance;
}

uint32_t ShaderRecompiler::SpvNewId() {
    return m_nextSpvId++;
}

void ShaderRecompiler::SpvEmit(std::vector<uint32_t>& out, uint32_t opcode, std::initializer_list<uint32_t> words) {
    uint32_t wordCount = 1 + (uint32_t)words.size();
    out.push_back((wordCount << 16) | opcode);
    for (auto w : words) out.push_back(w);
}

uint64_t ShaderRecompiler::ComputeShaderHash(const uint8_t* data, size_t size) {
    // FNV-1a 64-bit hash
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

ShaderStage ShaderRecompiler::DetectShaderStage(const uint8_t* data, size_t size) {
    if (size < 4) return ShaderStage::Unknown;

    // Verificar os primeiros bytes: cabeçalho SCE ou magic de compute
    uint32_t word0 = *reinterpret_cast<const uint32_t*>(data);

    // Shaders GNM/AGC começam com um uint32_t que codifica o tipo:
    // bits [3:0] = shader type (0=VS, 1=LS, 2=HS, 3=ES, 4=GS, 5=PS, 6=CS)
    uint8_t typeNibble = word0 & 0xF;
    switch (typeNibble) {
        case 0: return ShaderStage::Vertex;
        case 5: return ShaderStage::Pixel;
        case 6: return ShaderStage::Compute;
        case 2: return ShaderStage::Hull;
        case 4: return ShaderStage::Geometry;
        default: break;
    }

    // Heurística alternativa: compute shaders terminam com S_ENDPGM num padrão diferente
    // Procura por instrução EXP (exclusiva de VS/PS)
    for (size_t i = 0; i + 4 <= size; i += 4) {
        uint32_t instr = *reinterpret_cast<const uint32_t*>(data + i);
        if ((instr & 0xFF000000) == 0xC4000000) return ShaderStage::Pixel; // EXP
    }

    return ShaderStage::Compute; // default fallback
}

bool ShaderRecompiler::EmitSpirvPrologue(ShaderStage stage, std::vector<uint32_t>& out) {
    m_nextSpvId = 1;

    // Header SPIR-V
    out.push_back(Spv::MagicNumber);
    out.push_back(Spv::Version_1_5);
    out.push_back(Spv::GeneratorMagic);
    out.push_back(0); // Bound (será atualizado depois)
    out.push_back(0); // Schema

    // Capabilities
    SpvEmit(out, Spv::OpCapability, { Spv::CapShader });

    // Memory model
    SpvEmit(out, Spv::OpMemoryModel, {
        Spv::AddressingModelLogical,
        Spv::MemoryModelGLSL450
    });

    return true;
}

bool ShaderRecompiler::TranslateRdna2Instructions(
    const uint8_t* isaData, size_t isaSize,
    ShaderStage stage, std::vector<uint32_t>& spirvOut)
{
    // -----------------------------------------------------------------------
    // Tradução RDNA2 → SPIR-V (Implementação estrutural Fase 4)
    // -----------------------------------------------------------------------
    // Nesta fase geramos o corpo funcional mínimo válido em SPIR-V que passa
    // na validação do Vulkan. A tradução instrução-a-instrução completa será
    // incrementada nas fases 5+.

    uint32_t idVoid       = SpvNewId();
    uint32_t idFloat      = SpvNewId();
    uint32_t idVec4       = SpvNewId();
    uint32_t idVoidFn     = SpvNewId();
    uint32_t idMain       = SpvNewId();
    uint32_t idInputPtr   = SpvNewId();
    uint32_t idOutputPtr  = SpvNewId();
    uint32_t idInput      = SpvNewId();
    uint32_t idOutput     = SpvNewId();
    uint32_t idLabel      = SpvNewId();
    uint32_t idLoaded     = SpvNewId();

    uint32_t execModel = (stage == ShaderStage::Vertex)  ? Spv::ExecutionModelVertex :
                         (stage == ShaderStage::Compute) ? Spv::ExecutionModelGLCompute :
                                                           Spv::ExecutionModelFragment;

    // OpEntryPoint
    // Codifica a string "main" como palavras uint32
    std::vector<uint32_t> entryWords = { execModel, idMain };
    // "main\0" = 0x6E69616D, 0x00000000 (little-endian pack)
    entryWords.push_back(0x6E69616D); // 'main'
    entryWords.push_back(0x00000000); // null terminator padded
    entryWords.push_back(idInput);
    entryWords.push_back(idOutput);
    uint32_t epWordCount = 1 + (uint32_t)entryWords.size();
    spirvOut.push_back((epWordCount << 16) | Spv::OpEntryPoint);
    for (auto w : entryWords) spirvOut.push_back(w);

    // OpExecutionMode (somente para fragment shaders)
    if (stage == ShaderStage::Pixel) {
        SpvEmit(spirvOut, Spv::OpExecutionMode, { idMain, Spv::ExecutionModeOriginUpperLeft });
    }

    // Decorations
    SpvEmit(spirvOut, Spv::OpDecorate, { idInput,  Spv::DecorationLocation, 0 });
    SpvEmit(spirvOut, Spv::OpDecorate, { idOutput, Spv::DecorationLocation, 0 });

    // Types
    SpvEmit(spirvOut, Spv::OpTypeVoid,    { idVoid });
    SpvEmit(spirvOut, Spv::OpTypeFloat,   { idFloat, 32 });
    SpvEmit(spirvOut, Spv::OpTypeVector,  { idVec4, idFloat, 4 });
    SpvEmit(spirvOut, Spv::OpTypePointer, { idInputPtr,  Spv::StorageInput,  idVec4 });
    SpvEmit(spirvOut, Spv::OpTypePointer, { idOutputPtr, Spv::StorageOutput, idVec4 });
    SpvEmit(spirvOut, Spv::OpTypeFunction,{ idVoidFn, idVoid });

    // Variables
    SpvEmit(spirvOut, Spv::OpVariable, { idInputPtr,  idInput,  Spv::StorageInput  });
    SpvEmit(spirvOut, Spv::OpVariable, { idOutputPtr, idOutput, Spv::StorageOutput });

    // Function body
    SpvEmit(spirvOut, Spv::OpFunction,    { idVoid, idMain, 0, idVoidFn });
    SpvEmit(spirvOut, Spv::OpLabel,       { idLabel });
    SpvEmit(spirvOut, Spv::OpLoad,        { idVec4, idLoaded, idInput });
    SpvEmit(spirvOut, Spv::OpStore,       { idOutput, idLoaded });
    SpvEmit(spirvOut, Spv::OpReturn,      {});
    SpvEmit(spirvOut, Spv::OpFunctionEnd, {});

    // Atualiza o Bound no header
    spirvOut[3] = m_nextSpvId;

    // Estatísticas de análise
    size_t instrCount = isaSize / 4;
    std::cout << "    ISA: " << instrCount << " instrucoes RDNA2 detectadas\n";

    uint32_t expCount = 0, sampleCount = 0, vmemCount = 0;
    for (size_t i = 0; i + 4 <= isaSize; i += 4) {
        uint32_t instr = *reinterpret_cast<const uint32_t*>(isaData + i);
        if ((instr & 0xFF000000) == 0xC4000000) expCount++;
        if ((instr & 0xFF800000) == 0xF0800000) sampleCount++;
        if ((instr & 0xFFC00000) == 0xE8C00000) vmemCount++;
    }
    std::cout << "    EXP=" << expCount
              << " IMAGE_SAMPLE=" << sampleCount
              << " TBUFFER=" << vmemCount << "\n";

    return true;
}

bool ShaderRecompiler::EmitSpirvEpilogue(ShaderStage stage, std::vector<uint32_t>& out) {
    // Epilogo já emitido inline no TranslateRdna2Instructions (FunctionEnd + Return)
    return true;
}

RecompiledShader ShaderRecompiler::Recompile(const uint8_t* shaderData, size_t dataSize, ShaderStage hint) {
    RecompiledShader result;

    if (!shaderData || dataSize < 16) {
        result.errorMessage = "Dados de shader invalidos ou muito pequenos";
        return result;
    }

    // 1. Verifica cache
    uint64_t hash = ComputeShaderHash(shaderData, dataSize);
    auto cached = m_shaderCache.find(hash);
    if (cached != m_shaderCache.end()) {
        m_cacheHits++;
        return cached->second;
    }

    // 2. Detecta tipo de shader
    result.stage = (hint != ShaderStage::Unknown) ? hint : DetectShaderStage(shaderData, dataSize);

    const char* stageName[] = {"Vertex","Pixel","Compute","Geometry","Hull","Domain","Unknown"};
    std::cout << "  [ShaderRecompiler] Compilando shader " << stageName[(int)result.stage]
              << " (" << dataSize << " bytes, hash=0x" << std::hex << hash << std::dec << ")\n";

    // 3. Gera SPIR-V
    std::vector<uint32_t> spirv;

    // Header do SPIR-V (preenchido pelo prologue)
    spirv.reserve(dataSize / 2); // estimativa
    // Os primeiros 5 words são o header — inseridos pelo prologue
    spirv.push_back(Spv::MagicNumber);
    spirv.push_back(Spv::Version_1_5);
    spirv.push_back(Spv::GeneratorMagic);
    spirv.push_back(0); // Bound — atualizado depois
    spirv.push_back(0); // Schema

    SpvEmit(spirv, Spv::OpCapability, { Spv::CapShader });
    SpvEmit(spirv, Spv::OpMemoryModel, { Spv::AddressingModelLogical, Spv::MemoryModelGLSL450 });

    if (!TranslateRdna2Instructions(shaderData, dataSize, result.stage, spirv)) {
        result.errorMessage = "Falha na traducao de instrucoes RDNA2";
        return result;
    }

    result.spirvCode = spirv;
    result.success = true;

    std::cout << "  [ShaderRecompiler] SPIR-V gerado: "
              << (result.spirvCode.size() * 4) << " bytes, "
              << result.spirvCode.size() << " words.\n";

    // 4. Armazena no cache
    m_shaderCache[hash] = result;
    return result;
}

const RecompiledShader* ShaderRecompiler::GetCached(uint64_t shaderHash) {
    auto it = m_shaderCache.find(shaderHash);
    if (it != m_shaderCache.end()) {
        m_cacheHits++;
        return &it->second;
    }
    return nullptr;
}

void ShaderRecompiler::LoadCacheFromDisk() {
    const std::string path = "cache/shader_cache.bin";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cout << "[ShaderRecompiler] Nenhum cache encontrado (" << path << ").\n";
        return;
    }
    uint64_t entryCount = 0;
    in.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));
    for (uint64_t i = 0; i < entryCount; ++i) {
        uint64_t hash = 0;
        in.read(reinterpret_cast<char*>(&hash), sizeof(hash));
        uint32_t stageVal = 0;
        in.read(reinterpret_cast<char*>(&stageVal), sizeof(stageVal));
        uint64_t codeSize = 0;
        in.read(reinterpret_cast<char*>(&codeSize), sizeof(codeSize));
        RecompiledShader sh;
        sh.stage = static_cast<ShaderStage>(stageVal);
        sh.spirvCode.resize(codeSize);
        in.read(reinterpret_cast<char*>(sh.spirvCode.data()), codeSize * sizeof(uint32_t));
        sh.success = true;
        m_shaderCache.emplace(hash, std::move(sh));
    }
    std::cout << "[ShaderRecompiler] Cache carregado (" << m_shaderCache.size() << " shaders).\n";
}

void ShaderRecompiler::SaveCacheToDisk() {
    const std::string path = "cache/shader_cache.bin";
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[ShaderRecompiler] Falha ao abrir " << path << " para escrita.\n";
        return;
    }
    uint64_t entryCount = m_shaderCache.size();
    out.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));
    for (const auto& kv : m_shaderCache) {
        out.write(reinterpret_cast<const char*>(&kv.first), sizeof(kv.first));
        uint32_t stage = static_cast<uint32_t>(kv.second.stage);
        out.write(reinterpret_cast<const char*>(&stage), sizeof(stage));
        uint64_t codeSize = kv.second.spirvCode.size();
        out.write(reinterpret_cast<const char*>(&codeSize), sizeof(codeSize));
        out.write(reinterpret_cast<const char*>(kv.second.spirvCode.data()), codeSize * sizeof(uint32_t));
    }
    std::cout << "[ShaderRecompiler] Cache salvo em " << path << " (" << entryCount << " shaders).\n";
}

void ShaderRecompiler::PreheatRenderStageCache(uint8_t* ebootBase, size_t ebootSize) {
    // Carrega cache persistente, se existir
    LoadCacheFromDisk();

    // Se FastLoad está ativado e já possuímos todos os estágios críticos, não recompila
    const bool fastLoadEnabled = ConfigManager::Get().GetConfig().fastLoad;
    if (fastLoadEnabled && GetCacheSize() >= 5) {
        std::cout << "[ShaderRecompiler] FastLoad habilitado – cache já contém todos os estágios críticos, pulando pré‑compilação.\n";
        return;
    }

    std::cout << "[ShaderRecompiler] Pre-aquecendo cache de shaders dos RenderStages identificados...\n";

    struct RenderStageInfo {
        const char* name;
        uint64_t physOffset;
        size_t estimatedSize;
        ShaderStage stage;
    };

    const RenderStageInfo kStages[] = {
        { "DeferredLightingGi_PS",    0x80d914d, 512, ShaderStage::Pixel },
        { "GiProbeTraceLinkedList_CS", 0x80723fb, 256, ShaderStage::Compute },
        { "TiledLighting_PS",          0x8076c5e, 512, ShaderStage::Pixel },
        { "SharpenUpscale_CS",         0x810f177, 256, ShaderStage::Compute },
        { "BloomFinal_PS",             0x80a8723, 256, ShaderStage::Pixel }
    };

    int successCount = 0;
    for (const auto& stage : kStages) {
        if (stage.physOffset + stage.estimatedSize > ebootSize) {
            std::cerr << "  [!] " << stage.name << " fora dos limites – pulando\n";
            continue;
        }
        std::cout << "  Pré-aquecendo: " << stage.name << " @ 0x" << std::hex << stage.physOffset << std::dec << "\n";
        auto result = Recompile(ebootBase + stage.physOffset, stage.estimatedSize, stage.stage);
        if (result.success) {
            ++successCount;
            std::cout << "    -> OK (" << (result.spirvCode.size() * 4) << " bytes SPIR-V)\n";
        } else {
            std::cerr << "    -> ERRO: " << result.errorMessage << "\n";
        }
    }

    std::cout << "[ShaderRecompiler] Cache pré-aquecido: " << successCount << "/" << (sizeof(kStages)/sizeof(kStages[0])) << " shaders compilados. Hits: " << m_cacheHits << "\n";

    // Carrega shaders externos do cache de inFAMOUS / PS4 se disponível
    const std::string extCacheDirs[] = {
        "cache/shaders",
        "D:/Jogos ps4/issn-windows/issn-windows/user/cache"
    };
    for (const auto& dir : extCacheDirs) {
        if (std::filesystem::exists(dir)) {
            LoadSpirvDirectory(dir, 1000);
            break;
        }
    }

    // Carrega shaders do AMD FSR 4 se disponível
    const std::string fsr4Dirs[] = {
        "fsr4_shaders",
        "D:/Jogos ps4/issn-windows/issn-windows/fsr4_shaders"
    };
    for (const auto& dir : fsr4Dirs) {
        if (std::filesystem::exists(dir)) {
            LoadFsr4Shaders(dir);
            break;
        }
    }

    // Salva cache para uso futuro (exceto quando FastLoad já está habilitado)
    if (!fastLoadEnabled) {
        SaveCacheToDisk();
    }
}

size_t ShaderRecompiler::LoadSpirvDirectory(const std::string& dirPath, size_t maxShaders) {
    namespace fs = std::filesystem;
    if (!fs::exists(dirPath)) {
        return 0;
    }

    size_t count = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dirPath, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".spv") continue;

        std::string filename = entry.path().filename().string();
        uint64_t hash = 0;
        try {
            size_t underscore = filename.find('_');
            std::string hexPart = (underscore != std::string::npos) ? filename.substr(0, underscore) : filename.substr(0, filename.rfind('.'));
            hash = std::stoull(hexPart, nullptr, 16);
        } catch (...) {
            hash = ComputeShaderHash(reinterpret_cast<const uint8_t*>(filename.data()), filename.size());
        }

        std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;
        size_t fileSize = (size_t)file.tellg();
        if (fileSize < 20 || (fileSize % 4 != 0)) continue;

        file.seekg(0, std::ios::beg);
        std::vector<uint32_t> spv(fileSize / 4);
        file.read(reinterpret_cast<char*>(spv.data()), fileSize);

        if (spv[0] != Spv::MagicNumber) continue;

        RecompiledShader s;
        s.stage = ShaderStage::Pixel;
        s.spirvCode = std::move(spv);
        s.success = true;
        s.hash = hash;
        m_shaderCache[hash] = std::move(s);
        count++;

        if (count >= maxShaders) break;
    }

    std::cout << "[ShaderRecompiler] [OK] Carregados " << count << " shaders SPIR-V externos de " << dirPath << " (Total no cache: " << m_shaderCache.size() << ")\n";
    return count;
}

bool ShaderRecompiler::LoadFsr4Shaders(const std::string& fsr4Dir) {
    namespace fs = std::filesystem;
    if (!fs::exists(fsr4Dir)) {
        return false;
    }

    size_t count = 0;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fsr4Dir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".spv") continue;

        std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;
        size_t fileSize = (size_t)file.tellg();
        if (fileSize < 20 || (fileSize % 4 != 0)) continue;

        file.seekg(0, std::ios::beg);
        std::vector<uint32_t> spv(fileSize / 4);
        file.read(reinterpret_cast<char*>(spv.data()), fileSize);

        if (spv[0] != Spv::MagicNumber) continue;

        std::string name = entry.path().filename().string();
        m_fsr4Shaders[name] = std::move(spv);
        count++;
    }

    std::cout << "[ShaderRecompiler] [OK] Carregados " << count << " passes computacionais do AMD FSR 4 de " << fsr4Dir << "\n";
    return count > 0;
}

const std::vector<uint32_t>* ShaderRecompiler::GetFsr4Pass(const std::string& passName) const {
    auto it = m_fsr4Shaders.find(passName);
    if (it != m_fsr4Shaders.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace AstroPort
