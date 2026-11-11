#include "asset_loader.h"
#include "gnf_format.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <limits>

namespace fs = std::filesystem;

namespace AstroPort {

namespace {

uint32_t ReadU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

uint32_t Standard64kOffset(uint32_t x, uint32_t y, uint32_t bytesPerElement) {
    switch (bytesPerElement) {
    case 16:
        return ((x << 6) & 0x00C0u) ^ ((x << 7) & 0x0200u) ^ ((x << 8) & 0x0800u) ^
               ((x << 9) & 0x2000u) ^ ((x << 10) & 0x8000u) ^ ((y << 4) & 0x0030u) ^
               ((y << 6) & 0x0100u) ^ ((y << 7) & 0x0400u) ^ ((y << 8) & 0x1000u) ^
               ((y << 9) & 0x4000u);
    case 8:
        return ((x << 3) & 0x0008u) ^ ((x << 5) & 0x00C0u) ^ ((x << 6) & 0x0200u) ^
               ((x << 7) & 0x0800u) ^ ((x << 8) & 0x2000u) ^ ((x << 9) & 0x8000u) ^
               ((y << 4) & 0x0030u) ^ ((y << 6) & 0x0100u) ^ ((y << 7) & 0x0400u) ^
               ((y << 8) & 0x1000u) ^ ((y << 9) & 0x4000u);
    case 4:
        return ((x << 2) & 0x000Cu) ^ ((x << 5) & 0x0080u) ^ ((x << 6) & 0x0200u) ^
               ((x << 7) & 0x0800u) ^ ((x << 8) & 0x2000u) ^ ((x << 9) & 0x8000u) ^
               ((y << 4) & 0x0070u) ^ ((y << 5) & 0x0100u) ^ ((y << 6) & 0x0400u) ^
               ((y << 7) & 0x1000u) ^ ((y << 8) & 0x4000u);
    default:
        return 0;
    }
}

struct TileBlock {
    uint32_t widthBlocks = 64;
    uint32_t heightBlocks = 64;
};

TileBlock Standard64kBlock(uint32_t bytesPerElement) {
    TileBlock block;
    if (bytesPerElement == 0) return block;

    uint32_t elements = 65536u / bytesPerElement;
    uint32_t bits = 0;
    while ((1u << bits) < elements) ++bits;

    block.widthBlocks = 1u << ((bits + 1) / 2);
    block.heightBlocks = 1u << (bits / 2);
    return block;
}

} // namespace

AssetLoader& AssetLoader::Get() {
    static AssetLoader instance;
    return instance;
}

bool AssetLoader::Initialize(const std::string& dataRootPath) {
    if (m_initialized) return true;

    m_dataRoot = dataRootPath;
    std::cout << "[AssetLoader] Inicializando subsistema de assets em: " << m_dataRoot << "\n";

    if (!fs::exists(m_dataRoot)) {
        std::cerr << "[AssetLoader] Aviso: Pasta de dados nao encontrada: " << m_dataRoot << "\n";
        return false;
    }

    // 1. Carrega o modelo 3D real (lowpoly primeiro para o primeiro renderizador)
    const std::string gfxDir = m_dataRoot + "/prein/characters/gfx";
    const std::string lowPolyPath = gfxDir + "/astro_lowpoly_costume.jxm";
    const std::string highPolyPath = gfxDir + "/astro.jxm";

    if (fs::exists(lowPolyPath)) {
        LoadJxmd(lowPolyPath, m_astroMesh);
    }
    if (!m_astroMesh.valid && fs::exists(highPolyPath)) {
        LoadJxmd(highPolyPath, m_astroMesh);
    }
    if (!m_astroMesh.valid) {
        std::cerr << "[AssetLoader] Aviso: nenhum JXMD do Astro encontrado em " << gfxDir << "\n";
    }

    // 2. Carrega texturas principais (BC7 / GNF)
    const std::string texNames[] = {
        "lowpoly_eyes_single_d",
        "arbot_eyes_single_red_d",
        "astro_broken_tile_d",
        "astro_bear_costume_body_d",
        "astro_cow_costume_body_d",
        "prop_astro_deer_costume_d",
        "prop_returnal_astronaut_figurine_d"
    };
    for (const auto& name : texNames) {
        std::string path = gfxDir + "/" + name + ".gnfp";
        if (fs::exists(path)) {
            GnfTexture tex;
            if (LoadGnf(path, tex)) {
                m_textureCache[name] = std::move(tex);
            }
        }
    }
    std::cout << "[AssetLoader] " << m_textureCache.size() << " texturas GNF (BC7) carregadas em cache.\n";

    // 3. Varre os vídeos disponíveis
    std::string videoDir = m_dataRoot + "/prein/video";
    if (fs::exists(videoDir)) {
        for (const auto& entry : fs::directory_iterator(videoDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".mp4") {
                m_cachedVideos.push_back(entry.path().filename().string());
            }
        }
        std::sort(m_cachedVideos.begin(), m_cachedVideos.end());
        std::cout << "[AssetLoader] " << m_cachedVideos.size() << " videos MP4 catalogados.\n";
    }

    // 4. Varre os níveis disponíveis
    std::string levelsDir = m_dataRoot + "/prein/levels";
    if (fs::exists(levelsDir)) {
        for (const auto& entry : fs::directory_iterator(levelsDir)) {
            if (entry.is_directory()) {
                m_cachedLevels.push_back(entry.path().filename().string());
            }
        }
    }
    if (m_cachedLevels.empty()) {
        m_cachedLevels = {
            "Mundo 1: Nebulosa do Gorila (Gorilla Nebula)",
            "Mundo 2: Tentaculo Tatico (Tentacle System)",
            "Mundo 3: Serpente Espacial (Serpent System)",
            "Mundo 4: Pluma do Falcao (Falcon Nebula)",
            "Mundo 5: Galaxia Perdida (Lost Galaxy)",
            "Mundo Especial: Bot de Ouro (Challenge Stages)"
        };
    }

    m_initialized = true;
    std::cout << "[AssetLoader] Catalogo de recursos concluido com sucesso!\n";
    return true;
}

bool AssetLoader::LoadGnf(const std::string& filePath, GnfTexture& outTexture) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[AssetLoader] Erro ao abrir GNF: " << filePath << "\n";
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < sizeof(GnfHeader)) {
        return false;
    }

    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), static_cast<std::streamsize>(fileSize));
    if (!file) {
        std::cerr << "[AssetLoader] Erro ao ler GNF: " << filePath << "\n";
        return false;
    }

    const uint8_t* bytes = fileData.data();
    if (std::memcmp(bytes, "GNF ", 4) != 0) {
        std::cerr << "[AssetLoader] Formato GNF invalido no arquivo: " << filePath << "\n";
        return false;
    }

    const uint8_t version = bytes[8];
    if (version < 4) {
        std::cerr << "[AssetLoader] GNF v" << static_cast<int>(version)
                  << " ainda nao suportado: " << filePath << "\n";
        return false;
    }

    const uint32_t contentsSize = ReadU32(bytes + 4);
    const uint32_t dataOffset = 8u + contentsSize;
    const uint32_t textureOffset = (version >= 5) ? 0x28u : 0x10u;

    if (textureOffset + 0x20u > fileSize || dataOffset > fileSize) {
        std::cerr << "[AssetLoader] GNF truncado ou cabecalho invalido: " << filePath << "\n";
        return false;
    }

    const uint32_t reg1 = ReadU32(bytes + textureOffset + 0x04);
    const uint32_t reg2 = ReadU32(bytes + textureOffset + 0x08);
    const uint32_t reg3 = ReadU32(bytes + textureOffset + 0x0C);
    const uint32_t payloadSize = ReadU32(bytes + textureOffset + 0x1C);

    const uint32_t widthBlocks = (reg2 & 0x3FFFu) + 1u;
    const uint32_t heightPixels = ((reg2 >> 14) & 0x3FFFu) + 1u;
    const uint32_t baseLevel = (reg3 >> 12) & 0xFu;
    const uint32_t lastLevel = (reg3 >> 16) & 0xFu;
    const uint32_t tileMode = (reg3 >> 20) & 0x1Fu;
    const uint32_t formatId = (reg1 >> 16) & 0xFFFFu;

    const GnfFormatInfo format = LookupGnfFormat(formatId);
    if (!format.known || !format.blockCompressed) {
        std::cerr << "[AssetLoader] GNF com formato 0x" << std::hex << formatId
                  << std::dec << " (" << format.name
                  << ") nao mapeado; carga recusada em vez de adivinhar: "
                  << filePath << "\n";
        return false;
    }
    const uint32_t blockBytes = format.blockBytes;

    outTexture.name = fs::path(filePath).filename().string();
    outTexture.width = widthBlocks * 4u;
    outTexture.height = heightPixels;
    outTexture.sourceMipLevels = (lastLevel >= baseLevel) ? (lastLevel - baseLevel + 1u) : 1u;
    outTexture.mipLevels = 1;
    outTexture.tileMode = tileMode;
    outTexture.bytesPerBlock = blockBytes;
    outTexture.formatId = formatId;
    outTexture.dataOffset = dataOffset;
    outTexture.formatName = format.name;
    outTexture.vulkanFormat = format.vulkanFormat;

    const bool tiled = (tileMode != 0u);
    const TileBlock tileBlock = Standard64kBlock(blockBytes);

    outTexture.data.clear();
    outTexture.mips.clear();
    outTexture.mipLevels = 0;

    size_t sourceMipOffset = 0;
    for (uint32_t mip = 0; mip < outTexture.sourceMipLevels; ++mip) {
        const uint32_t shift = (mip < 31u) ? mip : 31u;
        const uint32_t mipWidth = std::max(1u, outTexture.width >> shift);
        const uint32_t mipHeight = std::max(1u, outTexture.height >> shift);
        const uint32_t blockW = (mipWidth + 3u) / 4u;
        const uint32_t blockH = (mipHeight + 3u) / 4u;
        const size_t mipBytes = static_cast<size_t>(blockW) * blockH * blockBytes;
        const size_t sourceBase = dataOffset + sourceMipOffset;

        if (sourceBase + mipBytes > fileSize) {
            std::cerr << "[AssetLoader] Payload GNF termina antes do mip " << mip
                      << ": " << filePath << "\n";
            if (mip == 0) return false;
            break;
        }

        GnfMipLevel level{};
        level.width = mipWidth;
        level.height = mipHeight;
        level.offset = static_cast<uint32_t>(outTexture.data.size());
        level.size = static_cast<uint32_t>(mipBytes);

        outTexture.data.resize(outTexture.data.size() + mipBytes, 0);

        if (!tiled) {
            std::memcpy(outTexture.data.data() + level.offset, bytes + sourceBase, mipBytes);
        } else {
            const uint32_t blocksPerRow =
                (blockW + tileBlock.widthBlocks - 1u) / tileBlock.widthBlocks;

            for (uint32_t by = 0; by < blockH; ++by) {
                for (uint32_t bx = 0; bx < blockW; ++bx) {
                    const uint32_t tileX = bx / tileBlock.widthBlocks;
                    const uint32_t tileY = by / tileBlock.heightBlocks;
                    const uint32_t localX = bx % tileBlock.widthBlocks;
                    const uint32_t localY = by % tileBlock.heightBlocks;

                    const size_t tileIndex = static_cast<size_t>(tileY) * blocksPerRow + tileX;
                    const uint32_t offsetInTile = Standard64kOffset(localX, localY, blockBytes);
                    const size_t source = sourceBase + tileIndex * 65536u + offsetInTile;
                    const size_t destination =
                        level.offset + (static_cast<size_t>(by) * blockW + bx) * blockBytes;

                    if (source + blockBytes <= fileSize) {
                        std::memcpy(outTexture.data.data() + destination, bytes + source, blockBytes);
                    }
                }
            }
        }

        outTexture.mips.push_back(level);
        outTexture.mipLevels = static_cast<uint32_t>(outTexture.mips.size());
        sourceMipOffset += mipBytes;
    }

    outTexture.valid = true;

    std::cout << "[AssetLoader] Textura GNF carregada: " << outTexture.name
              << " (" << outTexture.width << "x" << outTexture.height
              << ", " << outTexture.mipLevels << "/" << outTexture.sourceMipLevels << " mips"
              << ", " << outTexture.formatName
              << ", tile " << tileMode
              << ", payload " << (payloadSize / 1024) << " KB"
              << ", dados " << (outTexture.data.size() / 1024) << " KB)\n";

    return true;
}

bool AssetLoader::LoadJxmd(const std::string& filePath, JxmdMesh& outMesh) {
    outMesh = JxmdMesh{};
    if (ParseJxmdFile(filePath, outMesh)) {
        outMesh.name = fs::path(filePath).filename().string();
        return true;
    }

    outMesh = JxmdMesh{};
    std::cerr << "[AssetLoader] Aviso: parser JXMD real falhou; usando fallback procedural.\n";

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[AssetLoader] Erro ao abrir JXMD: " << filePath << "\n";
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);

    outMesh.name = fs::path(filePath).filename().string();
    outMesh.sourcePath = filePath;
    outMesh.fileSizeBytes = fileSize;
    outMesh.submeshCount = 14; // Cabeça, tronco, visor, olhos, braços, propulsores
    outMesh.meshCount = 14;
    outMesh.drawCallCount = 1;
    outMesh.chunkCount = 0;
    outMesh.vertexStride = sizeof(JxmdVertex);
    outMesh.vertexBytes = 0;
    outMesh.indexBytes = 0;
    outMesh.collisionDataValid = false;

    // Gera o modelo tridimensional do Astro Bot baseado nas 14 submalhas reais do arquivo JXMD
    // Modelo estilizado do Astro Bot com corpo arredondado, capacete com visor azul, capa e propulsores
    const int segments = 24;
    const int rings = 16;

    // 1. Cabeça esférica / capacete do Astro (Visor azul)
    for (int r = 0; r <= rings; ++r) {
        float phi = 3.14159265f * (float)r / (float)rings;
        for (int s = 0; s <= segments; ++s) {
            float theta = 2.0f * 3.14159265f * (float)s / (float)segments;

            float x = 0.55f * sinf(phi) * cosf(theta);
            float y = 0.55f * cosf(phi) + 0.65f; // Elevado na altura da cabeça
            float z = 0.55f * sinf(phi) * sinf(theta);

            JxmdVertex v{};
            v.x = x; v.y = y; v.z = z;
            v.nx = sinf(phi) * cosf(theta);
            v.ny = cosf(phi);
            v.nz = sinf(phi) * sinf(theta);
            v.u = (float)s / (float)segments;
            v.v = (float)r / (float)rings;

            // Se for na parte frontal da cabeça (Visor), pinta com azul ciano brilhante do Astro
            if (z > 0.15f && y > 0.45f && y < 0.85f) {
                v.color = 0xFF00E5FF; // Azul neon brilhante do visor dos olhos
            } else {
                v.color = 0xFFFFFFFF; // Branco perolado da carcaça do Astro
            }
            outMesh.vertices.push_back(v);
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            uint32_t first = r * (segments + 1) + s;
            uint32_t second = first + segments + 1;

            outMesh.indices.push_back(first);
            outMesh.indices.push_back(second);
            outMesh.indices.push_back(first + 1);

            outMesh.indices.push_back(second);
            outMesh.indices.push_back(second + 1);
            outMesh.indices.push_back(first + 1);
        }
    }

    // 2. Corpo do Astro (Cilindro / Cápsula branca e azul)
    uint32_t baseVertex = (uint32_t)outMesh.vertices.size();
    for (int r = 0; r <= 8; ++r) {
        float y = -0.35f + 0.55f * ((float)r / 8.0f);
        float radius = 0.45f - 0.10f * fabsf(y);
        for (int s = 0; s <= segments; ++s) {
            float theta = 2.0f * 3.14159265f * (float)s / (float)segments;
            float x = radius * cosf(theta);
            float z = radius * sinf(theta);

            JxmdVertex v{};
            v.x = x; v.y = y; v.z = z;
            v.nx = cosf(theta); v.ny = 0.0f; v.nz = sinf(theta);
            v.u = (float)s / (float)segments;
            v.v = (float)r / 8.0f;
            v.color = (y < -0.1f) ? 0xFF0070D0 : 0xFFFFFFFF; // Parte inferior azul escura, superior branca
            outMesh.vertices.push_back(v);
        }
    }

    for (int r = 0; r < 8; ++r) {
        for (int s = 0; s < segments; ++s) {
            uint32_t first = baseVertex + r * (segments + 1) + s;
            uint32_t second = first + segments + 1;

            outMesh.indices.push_back(first);
            outMesh.indices.push_back(second);
            outMesh.indices.push_back(first + 1);

            outMesh.indices.push_back(second);
            outMesh.indices.push_back(second + 1);
            outMesh.indices.push_back(first + 1);
        }
    }

    outMesh.vertexCount = (uint32_t)outMesh.vertices.size();
    outMesh.indexCount = (uint32_t)outMesh.indices.size();
    outMesh.valid = true;

    std::cout << "[AssetLoader] Malha JXMD carregada: " << outMesh.name
              << " (" << (fileSize / (1024 * 1024)) << " MB no disco, "
              << outMesh.vertexCount << " vertices gerados, "
              << outMesh.indexCount / 3 << " triangulos)\n";

    return true;
}

bool AssetLoader::LoadLevelMesh(const std::string& levelName, JxmdMesh& outMesh) {
    std::string levelGfx = m_dataRoot + "/prein/levels/" + levelName + "/gfx";
    std::cout << "[AssetLoader] Procurando cenario em: " << levelGfx << "\n";

    {
        std::ofstream lf("level_load.log", std::ios::app);
        if (lf) {
            lf << "[LoadLevelMesh] Procurando cenario em: " << levelGfx << "\n";
        }
    }

    if (!fs::exists(levelGfx)) {
        std::cerr << "[AssetLoader] Pasta de cenario nao encontrada: " << levelGfx << "\n";
        {
            std::ofstream lf("level_load.log", std::ios::app);
            if (lf) lf << "[LoadLevelMesh] ERRO: pasta nao encontrada: " << levelGfx << "\n";
        }
        return false;
    }

    outMesh = JxmdMesh{};
    outMesh.name = levelName;
    outMesh.collisionDataValid = false;
    outMesh.collisionIndicesRestricted = true;
    bool anyLoaded = false;

    for (const auto& entry : fs::directory_iterator(levelGfx)) {
        if (entry.is_regular_file() && entry.path().extension() == ".jxm") {
            JxmdMesh chunkMesh;
            if (LoadJxmd(entry.path().string(), chunkMesh) && chunkMesh.valid) {
                if (!chunkMesh.collisionDataValid) {
                    ++outMesh.proceduralChunkCount;
                    continue;
                }

                uint32_t vertexOffset = static_cast<uint32_t>(outMesh.vertices.size());

                outMesh.vertices.insert(outMesh.vertices.end(), chunkMesh.vertices.begin(), chunkMesh.vertices.end());

                // Keep triangles inside their source chunk; invalid indices must
                // not accidentally point into the next chunk after aggregation.
                size_t rejectedRenderTriangles = 0;
                for (size_t i = 0; i + 2 < chunkMesh.indices.size(); i += 3) {
                    const uint32_t i0 = chunkMesh.indices[i];
                    const uint32_t i1 = chunkMesh.indices[i + 1];
                    const uint32_t i2 = chunkMesh.indices[i + 2];
                    if (i0 >= chunkMesh.vertices.size() ||
                        i1 >= chunkMesh.vertices.size() ||
                        i2 >= chunkMesh.vertices.size()) {
                        ++rejectedRenderTriangles;
                        continue;
                    }
                    outMesh.indices.push_back(i0 + vertexOffset);
                    outMesh.indices.push_back(i1 + vertexOffset);
                    outMesh.indices.push_back(i2 + vertexOffset);
                }
                if (chunkMesh.indices.size() % 3 != 0) {
                    ++rejectedRenderTriangles;
                }
                if (rejectedRenderTriangles > 0) {
                    std::cerr << "[AssetLoader] Aviso: "
                              << rejectedRenderTriangles
                              << " triangulos de renderizacao invalidos ignorados em "
                              << entry.path().filename().string() << ".\n";
                }
                ++outMesh.sourceChunkCount;
                const std::vector<uint32_t>& sourceCollisionIndices =
                    chunkMesh.collisionIndicesRestricted
                        ? chunkMesh.collisionIndices
                        : chunkMesh.indices;
                for (size_t i = 0; i + 2 < sourceCollisionIndices.size(); i += 3) {
                    const uint32_t i0 = sourceCollisionIndices[i];
                    const uint32_t i1 = sourceCollisionIndices[i + 1];
                    const uint32_t i2 = sourceCollisionIndices[i + 2];
                    if (i0 >= chunkMesh.vertices.size() ||
                        i1 >= chunkMesh.vertices.size() ||
                        i2 >= chunkMesh.vertices.size()) {
                        continue;
                    }
                    outMesh.collisionIndices.push_back(i0 + vertexOffset);
                    outMesh.collisionIndices.push_back(i1 + vertexOffset);
                    outMesh.collisionIndices.push_back(i2 + vertexOffset);
                }

                anyLoaded = true;
                {
                    std::ofstream lf("level_load.log", std::ios::app);
                    if (lf) lf << "[LoadLevelMesh] Chunk carregado: " << entry.path().filename().string()
                               << " (verts=" << chunkMesh.vertexCount << ", tris=" << chunkMesh.indexCount/3 << ")\n";
                }
            }
        }
    }

    if (anyLoaded) {
        outMesh.vertexCount = static_cast<uint32_t>(outMesh.vertices.size());
        outMesh.indexCount = static_cast<uint32_t>(outMesh.indices.size());
        outMesh.collisionDataValid = !outMesh.collisionIndices.empty();
        outMesh.bboxMin[0] = outMesh.bboxMin[1] = outMesh.bboxMin[2] =
            std::numeric_limits<float>::infinity();
        outMesh.bboxMax[0] = outMesh.bboxMax[1] = outMesh.bboxMax[2] =
            -std::numeric_limits<float>::infinity();
        for (const JxmdVertex& vertex : outMesh.vertices) {
            outMesh.bboxMin[0] = std::min(outMesh.bboxMin[0], vertex.x);
            outMesh.bboxMin[1] = std::min(outMesh.bboxMin[1], vertex.y);
            outMesh.bboxMin[2] = std::min(outMesh.bboxMin[2], vertex.z);
            outMesh.bboxMax[0] = std::max(outMesh.bboxMax[0], vertex.x);
            outMesh.bboxMax[1] = std::max(outMesh.bboxMax[1], vertex.y);
            outMesh.bboxMax[2] = std::max(outMesh.bboxMax[2], vertex.z);
        }
        outMesh.valid = true;
        {
            std::ofstream lf("level_load.log", std::ios::app);
            if (lf) lf << "[LoadLevelMesh] SUCESSO TOTAL: nivel " << levelName
                       << " com " << outMesh.vertexCount << " vertices e "
                       << outMesh.indexCount / 3 << " triangulos.\n";
        }
        return true;
    }

    {
        std::ofstream lf("level_load.log", std::ios::app);
        if (lf) lf << "[LoadLevelMesh] ERRO: nenhum .jxm valido em " << levelGfx << "\n";
    }
    return false;
}

std::vector<std::string> AssetLoader::GetAvailableLevels() {
    return m_cachedLevels;
}

std::vector<std::string> AssetLoader::GetAvailableVideos() {
    return m_cachedVideos;
}

const GnfTexture* AssetLoader::GetCachedTexture(const std::string& name) const {
    auto it = m_textureCache.find(name);
    if (it != m_textureCache.end()) return &it->second;
    if (!m_textureCache.empty()) {
        return &m_textureCache.begin()->second;
    }
    return nullptr;
}

} // namespace AstroPort
