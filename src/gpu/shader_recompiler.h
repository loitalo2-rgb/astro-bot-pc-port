#pragma once

// ==============================================================================
// shader_recompiler.h — Tradutor de Shaders AGC (PS5 RDNA2) → SPIR-V / GLSL
//
// A GPU do PS5 usa a arquitetura AMD RDNA2 com a API "AGC" (Autonomous Graphic
// Core). Os shaders são compilados para ISA AMD GCN/RDNA2 e empacotados nos
// dados do jogo. Esta camada realiza:
//
//  1. Identificação do tipo de shader (VS, PS, CS) via cabeçalho SCE
//  2. Análise das instruções RDNA2 e extração de metadados (I/O, samplers)
//  3. Geração de SPIR-V equivalente para uso no Vulkan 1.3
//  4. Cache de pipelines compilados para evitar stuttering
// ==============================================================================

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace AstroPort {

// Tipos de shader suportados pelo pipeline AGC do PS5
enum class ShaderStage : uint8_t {
    Vertex          = 0,  // VS: Transformação de vértices
    Pixel           = 1,  // PS/FS: Fragmento (saída de cor)
    Compute         = 2,  // CS: Compute shader (iluminação tiled, upscale)
    Geometry        = 3,  // GS: Geometry shader
    Hull            = 4,  // HS: Hull (tessellation)
    Domain          = 5,  // DS: Domain (tessellation)
    Unknown         = 255
};

// Cabeçalho de shader SCE GNM/AGC no binário do jogo
struct SceShaderHeader {
    uint32_t magic;           // 0x5343455F ("SCE_") ou 0x434F4D50 ("COMP")
    uint32_t version;         // Versão do compilador
    uint8_t  shaderType;      // 0=VS, 1=PS, 2=CS, ...
    uint8_t  flags;
    uint16_t numInputSemantics;
    uint16_t numOutputSemantics;
    uint16_t numSamplers;
    uint16_t numBuffers;
    uint32_t codeSize;        // Tamanho da ISA em bytes (após este header)
    uint32_t dataSize;        // Tabela de constantes e metadados
};

// Semantic de entrada/saída (atributos do vertex shader / pixel shader)
struct ShaderSemantic {
    uint8_t  usage;           // POSITION, TEXCOORD, NORMAL, COLOR, etc.
    uint8_t  usageIndex;      // Índice (ex: TEXCOORD0, TEXCOORD1)
    uint8_t  regIndex;        // Registrador VGPR/SGPR alvo
    uint8_t  numComponents;   // Número de floats (1–4)
};

// Resultado da tradução de um único shader
struct RecompiledShader {
    ShaderStage stage;
    std::vector<uint32_t> spirvCode;    // Código SPIR-V gerado
    std::string entryPoint = "main";
    bool success = false;
    uint64_t hash = 0;
    std::string errorMessage;

    // Metadados para criação do VkPipeline
    uint32_t numInputAttributes = 0;
    uint32_t numOutputAttributes = 0;
    uint32_t pushConstantSize = 0;
    std::vector<uint32_t> descriptorSetLayouts;
};

// Par de shaders VS+PS formando um pipeline gráfico
struct GraphicsPipelineShaders {
    RecompiledShader vertexShader;
    RecompiledShader pixelShader;
    bool complete = false;
};

class ShaderRecompiler {
public:
    // Persistência em disco
    void LoadCacheFromDisk();
    void SaveCacheToDisk();

public:
    static ShaderRecompiler& Get();

    // Recompila um shader binário SCE/AGC para SPIR-V
    RecompiledShader Recompile(const uint8_t* shaderData, size_t dataSize, ShaderStage hint = ShaderStage::Unknown);

    // Obtém shader do cache (evita recompilação a cada frame)
    const RecompiledShader* GetCached(uint64_t shaderHash);

    // Pré-aquece o cache com os render stages identificados no binário
    void PreheatRenderStageCache(uint8_t* ebootBase, size_t ebootSize);

    // Carrega shaders SPIR-V (.spv) pré-compilados diretamente de um diretório de cache
    size_t LoadSpirvDirectory(const std::string& dirPath, size_t maxShaders = 1000);

    // Carrega os shaders compute do AMD FSR 4
    bool LoadFsr4Shaders(const std::string& fsr4Dir);
    const std::vector<uint32_t>* GetFsr4Pass(const std::string& passName) const;

    size_t GetCacheSize() const { return m_shaderCache.size(); }
    size_t GetCacheHits() const { return m_cacheHits; }
    size_t GetFsr4ShaderCount() const { return m_fsr4Shaders.size(); }

private:
    ShaderRecompiler() = default;

    // Cache: hash SHA1/FNV64 do bytecode → shader compilado
    std::unordered_map<uint64_t, RecompiledShader> m_shaderCache;
    std::unordered_map<std::string, std::vector<uint32_t>> m_fsr4Shaders;
    size_t m_cacheHits = 0;

    // Passos de tradução
    ShaderStage  DetectShaderStage(const uint8_t* data, size_t size);
    uint64_t     ComputeShaderHash(const uint8_t* data, size_t size);
    bool         ParseSceShaderHeader(const uint8_t* data, size_t size, SceShaderHeader& outHeader);
    bool         EmitSpirvPrologue(ShaderStage stage, std::vector<uint32_t>& out);
    bool         TranslateRdna2Instructions(const uint8_t* isaData, size_t isaSize,
                                            ShaderStage stage, std::vector<uint32_t>& spirvOut);
    bool         EmitSpirvEpilogue(ShaderStage stage, std::vector<uint32_t>& out);

    // Geração SPIR-V estrutural (sem dependência de library externa nesta fase)
    void         SpvEmit(std::vector<uint32_t>& out, uint32_t opcode, std::initializer_list<uint32_t> words);
    uint32_t     SpvNewId();

    uint32_t m_nextSpvId = 1;
};

} // namespace AstroPort
