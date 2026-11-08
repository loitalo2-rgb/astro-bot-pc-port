#pragma once

#include "jxmd_parser.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace AstroPort {

// ==============================================================================
// GNF (Sony Gnm Format) — Textura Nativa PS4 / PS5
// ==============================================================================
#pragma pack(push, 1)
struct GnfHeader {
    char     magic[4];       // "GNF " (0x20464E47)
    uint32_t contentsSize;   // Tamanho da area de cabecalho/descritores
    uint8_t  version;        // Versao do conteiner GNF (4 ou 5)
    uint8_t  numTextures;
    uint8_t  alignment;
    uint8_t  reserved;
};

struct GnfSurfaceInfo {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t numMipLevels;
    uint32_t surfaceFormat;  // Formato GNM/PS5 (ex.: BC7, BC4)
    uint32_t channelOrder;
    uint32_t tileMode;
    uint32_t pitch;
    uint32_t formatId;
    uint32_t bytesPerBlock;
    uint32_t dataOffset;
    uint32_t payloadSize;
};
#pragma pack(pop)

struct GnfMipLevel {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t offset = 0; // Offset em GnfTexture.data
    uint32_t size = 0;
};

struct GnfTexture {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;       // Mips realmente carregados em data
    uint32_t sourceMipLevels = 1; // Mips presentes no arquivo GNF
    uint32_t tileMode = 0;
    uint32_t bytesPerBlock = 0;
    uint32_t formatId = 0;
    uint32_t dataOffset = 0;
    std::string formatName;
    uint32_t vulkanFormat = 0; // VkFormat correspondente
    std::vector<uint8_t> data;
    std::vector<GnfMipLevel> mips;
    bool valid = false;
};

// ==============================================================================
// AssetLoader — Carregador de Assets da Engine
// ==============================================================================
class AssetLoader {
public:
    static AssetLoader& Get();

    // Carregamento de Texturas GNF
    bool LoadGnf(const std::string& filePath, GnfTexture& outTexture);

    // Carregamento de Malhas JXMD e Cenários Reais
    bool LoadJxmd(const std::string& filePath, JxmdMesh& outMesh);
    bool LoadLevelMesh(const std::string& levelName, JxmdMesh& outMesh);

    // Varredura de Recursos no Disco
    std::vector<std::string> GetAvailableLevels();
    std::vector<std::string> GetAvailableVideos();

    // Consultas de Cache
    const JxmdMesh* GetCachedAstroMesh() const { return m_astroMesh.valid ? &m_astroMesh : nullptr; }
    const GnfTexture* GetCachedTexture(const std::string& name) const;
    size_t GetLoadedTextureCount() const { return m_textureCache.size(); }

    bool Initialize(const std::string& dataRootPath);

private:
    AssetLoader() = default;

    std::string m_dataRoot;
    bool m_initialized = false;
    JxmdMesh m_astroMesh;
    std::unordered_map<std::string, GnfTexture> m_textureCache;
    std::vector<std::string> m_cachedLevels;
    std::vector<std::string> m_cachedVideos;
};

} // namespace AstroPort
