#include "jxmd_parser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace AstroPort {
namespace {

constexpr uint32_t kChunkHeaderSize = 0x20;
constexpr uint32_t kDefaultVertexStride = 44;
constexpr float kDefaultPositionScale = 1.0f / 8192.0f;

uint32_t ReadU32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint16_t ReadU16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    return static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
}

int16_t ReadI16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<int16_t>(ReadU16(data, offset));
}

bool IsChunkMagic(const uint8_t* p) {
    for (int i = 0; i < 4; ++i) {
        if (p[i] < 'A' || p[i] > 'Z') return false;
    }
    return true;
}

std::string MagicToString(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return "????";
    return std::string(reinterpret_cast<const char*>(data.data() + offset), 4);
}

float HalfToFloat(uint16_t half) {
    const uint32_t sign = (half >> 15) & 0x1;
    const uint32_t exponent = (half >> 10) & 0x1F;
    const uint32_t mantissa = half & 0x3FF;

    if (exponent == 0) {
        if (mantissa == 0) return sign ? -0.0f : 0.0f;
        const float value = std::ldexp(static_cast<float>(mantissa), -24);
        return sign ? -value : value;
    }
    if (exponent == 31) {
        if (mantissa == 0) return sign ? -INFINITY : INFINITY;
        return NAN;
    }

    const float value = std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f,
                                   static_cast<int>(exponent) - 15);
    return sign ? -value : value;
}

bool ReadFile(const std::string& filePath, std::vector<uint8_t>& out) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) return false;
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(out.data()), size);
    return file.good() || file.eof();
}

std::vector<JxmdChunkInfo> WalkChunks(const std::vector<uint8_t>& data) {
    std::vector<JxmdChunkInfo> chunks;
    size_t offset = 0;

    while (offset + kChunkHeaderSize <= data.size()) {
        if (!IsChunkMagic(data.data() + offset)) break;

        JxmdChunkInfo chunk;
        chunk.magic = MagicToString(data, offset);
        chunk.offset = static_cast<uint32_t>(offset);
        chunk.size = ReadU32(data, offset + 4);
        chunk.payloadOffset = ReadU32(data, offset + 12);
        chunk.hash = ReadU32(data, offset + 16);
        chunk.count = ReadU32(data, offset + 20);
        chunk.stride = ReadU32(data, offset + 24);
        chunk.flags = ReadU32(data, offset + 28);

        if (chunk.size < kChunkHeaderSize || offset + chunk.size > data.size()) {
            std::cerr << "[JXMD] Chunk invalido em 0x" << std::hex << offset
                      << std::dec << " (" << chunk.magic << ", size 0x"
                      << std::hex << chunk.size << std::dec << "), parando.\n";
            break;
        }

        chunks.push_back(chunk);
        offset += chunk.size;
    }

    return chunks;
}

const JxmdChunkInfo* FindChunk(const std::vector<JxmdChunkInfo>& chunks, const std::string& magic) {
    for (const auto& chunk : chunks) {
        if (chunk.magic == magic) return &chunk;
    }
    return nullptr;
}

uint32_t ChunkEntryCount(const JxmdChunkInfo& chunk) {
    if (chunk.stride == 0 || chunk.payloadOffset >= chunk.size) return 0;
    return (chunk.size - chunk.payloadOffset) / chunk.stride;
}

void ComputeNormals(JxmdMesh& mesh) {
    std::vector<float> nx(mesh.vertices.size(), 0.0f);
    std::vector<float> ny(mesh.vertices.size(), 0.0f);
    std::vector<float> nz(mesh.vertices.size(), 0.0f);

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t i0 = mesh.indices[i];
        const uint32_t i1 = mesh.indices[i + 1];
        const uint32_t i2 = mesh.indices[i + 2];
        if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size()) {
            continue;
        }

        const auto& a = mesh.vertices[i0];
        const auto& b = mesh.vertices[i1];
        const auto& c = mesh.vertices[i2];

        const float e1x = b.x - a.x;
        const float e1y = b.y - a.y;
        const float e1z = b.z - a.z;
        const float e2x = c.x - a.x;
        const float e2y = c.y - a.y;
        const float e2z = c.z - a.z;

        const float cx = e1y * e2z - e1z * e2y;
        const float cy = e1z * e2x - e1x * e2z;
        const float cz = e1x * e2y - e1y * e2x;

        nx[i0] += cx; ny[i0] += cy; nz[i0] += cz;
        nx[i1] += cx; ny[i1] += cy; nz[i1] += cz;
        nx[i2] += cx; ny[i2] += cy; nz[i2] += cz;
    }

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const float length = std::sqrt(nx[i] * nx[i] + ny[i] * ny[i] + nz[i] * nz[i]);
        if (length > 1e-8f) {
            mesh.vertices[i].nx = nx[i] / length;
            mesh.vertices[i].ny = ny[i] / length;
            mesh.vertices[i].nz = nz[i] / length;
        } else {
            mesh.vertices[i].nx = 0.0f;
            mesh.vertices[i].ny = 1.0f;
            mesh.vertices[i].nz = 0.0f;
        }
    }
}

} // namespace

bool ParseJxmdFile(const std::string& filePath, JxmdMesh& outMesh) {
    outMesh = JxmdMesh{};
    outMesh.name = filePath;
    outMesh.sourcePath = filePath;

    std::vector<uint8_t> data;
    if (!ReadFile(filePath, data)) {
        std::cerr << "[JXMD] Nao foi possivel ler: " << filePath << "\n";
        return false;
    }

    outMesh.fileSizeBytes = data.size();
    if (data.size() < kChunkHeaderSize || std::memcmp(data.data(), "JXMD", 4) != 0) {
        std::cerr << "[JXMD] Assinatura invalida em: " << filePath << "\n";
        return false;
    }

    const auto chunks = WalkChunks(data);
    outMesh.chunkCount = static_cast<uint32_t>(chunks.size());

    const JxmdChunkInfo* vb = FindChunk(chunks, "VBBD");
    const JxmdChunkInfo* ib = FindChunk(chunks, "IBBD");
    const JxmdChunkInfo* submesh = FindChunk(chunks, "SUBM");
    const JxmdChunkInfo* meshes = FindChunk(chunks, "MESH");
    const JxmdChunkInfo* geom = FindChunk(chunks, "GEOM");

    if (!vb || !ib) {
        std::cerr << "[JXMD] VBBD/IBBD ausentes em: " << filePath << "\n";
        return false;
    }

    const uint32_t vbPayload = (vb->payloadOffset >= kChunkHeaderSize && vb->payloadOffset < vb->size)
        ? vb->payloadOffset : kChunkHeaderSize;
    const uint32_t ibPayload = (ib->payloadOffset >= kChunkHeaderSize && ib->payloadOffset < ib->size)
        ? ib->payloadOffset : kChunkHeaderSize;

    const size_t vbStart = static_cast<size_t>(vb->offset) + vbPayload;
    const size_t ibStart = static_cast<size_t>(ib->offset) + ibPayload;
    const size_t vbBytes = static_cast<size_t>(vb->size) - vbPayload;
    const size_t ibBytes = static_cast<size_t>(ib->size) - ibPayload;

    if (vbStart + vbBytes > data.size() || ibStart + ibBytes > data.size()) {
        std::cerr << "[JXMD] Chunk de geometria fora dos limites: " << filePath << "\n";
        return false;
    }

    outMesh.vertexStride = kDefaultVertexStride;
    outMesh.positionScale = kDefaultPositionScale;
    outMesh.vertexBytes = vbBytes;
    outMesh.indexBytes = ibBytes;

    const uint32_t rawVertexCount = static_cast<uint32_t>(vbBytes / outMesh.vertexStride);
    const uint32_t rawIndexCount = static_cast<uint32_t>(ibBytes / sizeof(uint16_t));

    if (rawVertexCount == 0 || rawIndexCount == 0) {
        std::cerr << "[JXMD] Geometria vazia em: " << filePath << "\n";
        return false;
    }

    outMesh.vertices.reserve(rawVertexCount);
    for (uint32_t i = 0; i < rawVertexCount; ++i) {
        const size_t base = vbStart + static_cast<size_t>(i) * outMesh.vertexStride;
        JxmdVertex v;
        v.x = static_cast<float>(ReadI16(data, base + 0)) * outMesh.positionScale;
        v.y = static_cast<float>(ReadI16(data, base + 2)) * outMesh.positionScale;
        v.z = static_cast<float>(ReadI16(data, base + 4)) * outMesh.positionScale;

        // UV fica em 0x1C/0x1E; 0x18/0x1A contém outro atributo (tangente/normal).
        const float u = HalfToFloat(ReadU16(data, base + 28));
        const float w = HalfToFloat(ReadU16(data, base + 30));
        if (std::isfinite(u) && std::isfinite(w)) {
            v.u = u;
            v.v = w;
        }

        v.color = 0xFFFFFFFF;
        outMesh.vertices.push_back(v);
    }

    outMesh.indices.reserve(rawIndexCount);
    for (uint32_t i = 0; i < rawIndexCount; ++i) {
        const uint32_t index = ReadU16(data, ibStart + static_cast<size_t>(i) * sizeof(uint16_t));
        outMesh.indices.push_back(index);
    }

    // Alguns buffers de indice possuem padding 0x0000 no final.
    while (outMesh.indices.size() % 3 != 0 && !outMesh.indices.empty() && outMesh.indices.back() == 0) {
        outMesh.indices.pop_back();
    }
    if (outMesh.indices.size() % 3 != 0) {
        outMesh.indices.resize(outMesh.indices.size() - (outMesh.indices.size() % 3));
    }

    if (outMesh.indices.empty()) {
        std::cerr << "[JXMD] Nenhum indice valido em: " << filePath << "\n";
        return false;
    }

    if (geom && meshes) {
        const uint32_t geomCount = ChunkEntryCount(*geom);
        const uint32_t meshCount = ChunkEntryCount(*meshes);
        const uint32_t drawCount = std::min(geomCount, meshCount);
        const size_t geomBase = static_cast<size_t>(geom->offset) + geom->payloadOffset;
        const size_t meshBase = static_cast<size_t>(meshes->offset) + meshes->payloadOffset;

        outMesh.submeshes.reserve(drawCount);
        for (uint32_t i = 0; i < drawCount; ++i) {
            const size_t geomEntry = geomBase + static_cast<size_t>(i) * geom->stride;
            const size_t meshEntry = meshBase + static_cast<size_t>(i) * meshes->stride;

            JxmdSubmesh range;
            range.indexOffset = ReadU16(data, geomEntry + 8);
            range.indexCount = ReadU16(data, geomEntry + 20);
            range.materialIndex = ReadU16(data, meshEntry + 4);

            if (range.indexCount == 0) continue;
            if (static_cast<size_t>(range.indexOffset) + range.indexCount > outMesh.indices.size()) {
                std::cerr << "[JXMD] Submesh " << i << " fora dos limites de indices.\n";
                continue;
            }
            outMesh.submeshes.push_back(range);
        }
    }

    const JxmdChunkInfo* stbl = FindChunk(chunks, "STBL");
    const JxmdChunkInfo* stbd = FindChunk(chunks, "STBD");
    if (stbl && stbd) {
        const size_t stblBase = static_cast<size_t>(stbl->offset) + stbl->payloadOffset;
        const size_t stbdBase = static_cast<size_t>(stbd->offset) + stbd->payloadOffset;
        for (uint32_t i = 0; i < stbl->count; ++i) {
            const size_t entry = stblBase + static_cast<size_t>(i) * stbl->stride;
            uint32_t strOffset = ReadU32(data, entry + 0);
            uint32_t strLen = ReadU32(data, entry + 4);
            if (stbdBase + strOffset + strLen <= data.size()) {
                std::string s(reinterpret_cast<const char*>(data.data() + stbdBase + strOffset), strLen);
                if (!s.empty() && s.back() == '\0') s.pop_back();
                outMesh.strings.push_back(s);
            }
        }
    }

    const JxmdChunkInfo* texi = FindChunk(chunks, "TEXI");
    if (texi) {
        const size_t texiBase = static_cast<size_t>(texi->offset) + texi->payloadOffset;
        for (uint32_t i = 0; i < texi->count; ++i) {
            const size_t entry = texiBase + static_cast<size_t>(i) * texi->stride;
            JxmdTexture tex;
            tex.id = ReadU32(data, entry + 0);
            if (tex.id < outMesh.strings.size()) {
                tex.name = outMesh.strings[tex.id];
            }
            tex.width = ReadU32(data, entry + 4);
            tex.height = ReadU32(data, entry + 8);
            tex.format = ReadU32(data, entry + 12);
            outMesh.textures.push_back(tex);
        }
    }

    const JxmdChunkInfo* mati = FindChunk(chunks, "MATI");
    if (mati) {
        const size_t matiBase = static_cast<size_t>(mati->offset) + mati->payloadOffset;
        for (uint32_t i = 0; i < mati->count; ++i) {
            const size_t entry = matiBase + static_cast<size_t>(i) * mati->stride;
            JxmdMaterial mat;
            mat.id = ReadU32(data, entry + 0);
            for (size_t j = 4; j + 4 <= mati->stride; j += 4) {
                uint32_t texId = ReadU32(data, entry + j);
                if (texId != 0xFFFFFFFF && texId != 0) {
                     mat.textureIds.push_back(texId);
                }
            }
            outMesh.materials.push_back(mat);
        }
    }

    uint32_t maxIndex = 0;
    for (uint32_t index : outMesh.indices) {
        maxIndex = std::max(maxIndex, index);
    }
    if (static_cast<size_t>(maxIndex) + 1 < outMesh.vertices.size()) {
        outMesh.vertices.resize(static_cast<size_t>(maxIndex) + 1);
    } else if (maxIndex >= outMesh.vertices.size()) {
        std::cerr << "[JXMD] Aviso: indice " << maxIndex
                  << " excede o numero de vertices (" << outMesh.vertices.size() << ").\n";
    }

    outMesh.vertexCount = static_cast<uint32_t>(outMesh.vertices.size());
    outMesh.indexCount = static_cast<uint32_t>(outMesh.indices.size());
    outMesh.submeshCount = outMesh.submeshes.empty()
        ? (submesh ? submesh->count : 1)
        : static_cast<uint32_t>(outMesh.submeshes.size());
    outMesh.meshCount = meshes ? ChunkEntryCount(*meshes) : 1;
    outMesh.drawCallCount = outMesh.submeshes.empty() ? outMesh.meshCount
                                                       : static_cast<uint32_t>(outMesh.submeshes.size());

    outMesh.bboxMin[0] = outMesh.bboxMin[1] = outMesh.bboxMin[2] = 1e30f;
    outMesh.bboxMax[0] = outMesh.bboxMax[1] = outMesh.bboxMax[2] = -1e30f;
    for (const auto& v : outMesh.vertices) {
        outMesh.bboxMin[0] = std::min(outMesh.bboxMin[0], v.x);
        outMesh.bboxMin[1] = std::min(outMesh.bboxMin[1], v.y);
        outMesh.bboxMin[2] = std::min(outMesh.bboxMin[2], v.z);
        outMesh.bboxMax[0] = std::max(outMesh.bboxMax[0], v.x);
        outMesh.bboxMax[1] = std::max(outMesh.bboxMax[1], v.y);
        outMesh.bboxMax[2] = std::max(outMesh.bboxMax[2], v.z);
    }

    ComputeNormals(outMesh);
    outMesh.valid = true;

    std::cout << "[JXMD] Geometria real carregada: " << filePath << "\n"
              << "  -> Chunks: " << outMesh.chunkCount
              << " | VBO: " << vbBytes << " bytes"
              << " | IBO: " << ibBytes << " bytes\n"
              << "  -> Vertices: " << outMesh.vertexCount
              << " (stride " << outMesh.vertexStride << ")"
              << " | Indices: " << outMesh.indexCount
              << " | Draw calls: " << outMesh.drawCallCount
              << " | Submeshes: " << outMesh.submeshCount << "\n"
              << "  -> BBox: [" << outMesh.bboxMin[0] << ", " << outMesh.bboxMin[1]
              << ", " << outMesh.bboxMin[2] << "] .. ["
              << outMesh.bboxMax[0] << ", " << outMesh.bboxMax[1]
              << ", " << outMesh.bboxMax[2] << "]\n";

    return true;
}

std::string DumpJxmdFile(const std::string& filePath) {
    JxmdMesh mesh;
    std::vector<uint8_t> data;
    std::ostringstream out;

    out << "JXMD dump: " << filePath << "\n";
    if (!ReadFile(filePath, data)) {
        out << "ERRO: nao foi possivel ler o arquivo.\n";
        return out.str();
    }
    out << "Tamanho: " << data.size() << " bytes\n";

    const auto chunks = WalkChunks(data);
    out << "Chunks: " << chunks.size() << "\n";
    out << std::left << std::setw(6) << "magic"
        << std::setw(12) << "offset"
        << std::setw(12) << "size"
        << std::setw(10) << "payload"
        << std::setw(8) << "count"
        << std::setw(8) << "stride" << "\n";
    for (const auto& c : chunks) {
        out << std::left << std::setw(6) << c.magic
            << "0x" << std::hex << std::setw(8) << c.offset << std::dec
            << "0x" << std::hex << std::setw(8) << c.size << std::dec
            << std::setw(8) << c.payloadOffset
            << std::setw(8) << c.count
            << std::setw(8) << c.stride << "\n";
    }

    if (ParseJxmdFile(filePath, mesh)) {
        out << "\nGeometria:\n"
            << "  vertices: " << mesh.vertexCount << "\n"
            << "  indices:  " << mesh.indexCount << "\n"
            << "  stride:   " << mesh.vertexStride << "\n"
            << "  scale:    " << mesh.positionScale << "\n"
            << "  bbox min: " << mesh.bboxMin[0] << ", " << mesh.bboxMin[1] << ", " << mesh.bboxMin[2] << "\n"
            << "  bbox max: " << mesh.bboxMax[0] << ", " << mesh.bboxMax[1] << ", " << mesh.bboxMax[2] << "\n";
    } else {
        out << "\nERRO ao extrair geometria.\n";
    }

    return out.str();
}

} // namespace AstroPort
