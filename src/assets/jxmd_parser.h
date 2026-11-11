#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace AstroPort {

struct JxmdVertex {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float u = 0.0f, v = 0.0f;
    uint32_t color = 0xFFFFFFFF;
};

struct JxmdSubmesh {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

struct JxmdChunkInfo {
    std::string magic;
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t payloadOffset = 0;
    uint32_t hash = 0;
    uint32_t count = 0;
    uint32_t stride = 0;
    uint32_t flags = 0;
};

struct JxmdTexture {
    uint32_t id = 0;
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
};

struct JxmdMaterial {
    uint32_t id = 0;
    std::vector<uint32_t> textureIds; // Referências aos IDs em JxmdTexture
};

struct JxmdMesh {
    std::string name;
    std::string sourcePath;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t submeshCount = 0;
    uint32_t meshCount = 0;
    uint32_t drawCallCount = 0;
    uint32_t chunkCount = 0;
    uint32_t sourceChunkCount = 0;
    uint32_t proceduralChunkCount = 0;
    uint32_t vertexStride = 0;
    float positionScale = 1.0f / 8192.0f;
    size_t fileSizeBytes = 0;
    size_t vertexBytes = 0;
    size_t indexBytes = 0;
    float bboxMin[3] = { 0.0f, 0.0f, 0.0f };
    float bboxMax[3] = { 0.0f, 0.0f, 0.0f };
    std::vector<JxmdVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> collisionIndices;
    std::vector<JxmdSubmesh> submeshes;
    std::vector<JxmdTexture> textures;
    std::vector<JxmdMaterial> materials;
    std::vector<std::string> strings;
    bool collisionDataValid = true;
    bool collisionIndicesRestricted = false;
    bool valid = false;
};

// Le um arquivo JXMD real e extrai geometria indexada.
bool ParseJxmdFile(const std::string& filePath, JxmdMesh& outMesh);

// Gera um texto de diagnostico com os chunks, contagens e bounding box.
std::string DumpJxmdFile(const std::string& filePath);

} // namespace AstroPort
