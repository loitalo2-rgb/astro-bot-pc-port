#include "assets/asset_census.h"

#include "assets/gnf_format.h"

#include <cstring>
#include <limits>

namespace AstroPort {
namespace {

constexpr uint32_t kSelfMagicLe = 0x1D3D154Fu;
constexpr uint32_t kSpirvMagicLe = 0x07230203u;
constexpr size_t kGnfHeaderSize = 12;

uint32_t ReadU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

void FillMagic(const uint8_t* bytes, size_t size, AssetHeaderInfo& out) {
    const size_t count = size < 4 ? size : 4;
    out.magic.assign(reinterpret_cast<const char*>(bytes), count);
    for (char& c : out.magic) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 32 || u > 126) c = '.';
    }
}

// GNF header layout used by the project's loader: magic, contentsSize, version,
// texture count, alignment, then the first surface descriptor. Only values
// needed for the census are extracted; payload bytes are never touched.
bool ClassifyGnf(const uint8_t* bytes, size_t size, uint64_t fileSize,
                 AssetHeaderInfo& out) {
    out.kind = AssetKind::Gnf;
    if (size < kGnfHeaderSize) return true;

    out.gnfContentsSize = ReadU32(bytes + 4);
    out.gnfVersion = bytes[8];
    out.gnfTextures = bytes[9];

    const uint32_t dataOffset = 8u + out.gnfContentsSize;
    out.dataOffset = dataOffset;
    const uint32_t textureOffset = (out.gnfVersion >= 5) ? 0x28u : 0x10u;
    const uint64_t needed = static_cast<uint64_t>(textureOffset) + 0x20u;
    if (out.gnfVersion < 4 || needed > size ||
        (fileSize != 0 && dataOffset > fileSize)) {
        return true;  // truncated or unsupported: still a GNF by magic
    }

    const uint32_t reg1 = ReadU32(bytes + textureOffset + 0x04);
    const uint32_t reg2 = ReadU32(bytes + textureOffset + 0x08);
    const uint32_t reg3 = ReadU32(bytes + textureOffset + 0x0C);
    out.payloadSize = ReadU32(bytes + textureOffset + 0x1C);

    const uint32_t widthBlocks = (reg2 & 0x3FFFu) + 1u;
    const uint32_t heightPixels = ((reg2 >> 14) & 0x3FFFu) + 1u;
    const uint32_t baseLevel = (reg3 >> 12) & 0xFu;
    const uint32_t lastLevel = (reg3 >> 16) & 0xFu;
    out.tileMode = (reg3 >> 20) & 0x1Fu;
    out.formatId = (reg1 >> 16) & 0xFFFFu;
    out.width = widthBlocks * 4u;
    out.height = heightPixels;
    out.mipLevels = (lastLevel >= baseLevel) ? (lastLevel - baseLevel + 1u) : 1u;

    const bool isBc4 = (out.formatId == 0xCAF0u || out.formatId == 0x8AF0u);
    out.bytesPerBlock = isBc4 ? 8u : 16u;
    const GnfFormatInfo format = LookupGnfFormat(out.formatId);
    out.gnfFormatKnown = format.known;
    out.gnfFormatName = format.name;
    if (format.known) out.bytesPerBlock = format.blockBytes;
    out.valid = true;
    return true;
}

// JXMD files start with a chunk table; the first chunk header carries magic,
// size, payload offset, hash, element count and stride.
bool ClassifyJxmd(const uint8_t* bytes, size_t size, uint64_t fileSize,
                  AssetHeaderInfo& out) {
    out.kind = AssetKind::Jxmd;
    if (size < 0x20) return true;

    out.chunkSize = ReadU32(bytes + 4);
    out.chunkPayloadOffset = ReadU32(bytes + 12);
    out.chunkCount = ReadU32(bytes + 20);
    out.chunkStride = ReadU32(bytes + 24);
    out.chunkFlags = ReadU32(bytes + 28);

    out.valid = out.chunkSize >= 0x20 &&
                (fileSize == 0 ||
                 static_cast<uint64_t>(out.chunkSize) <= fileSize);
    return true;
}

} // namespace

bool ClassifyAssetHeader(const uint8_t* bytes, size_t size, uint64_t fileSize,
                         AssetHeaderInfo& out) {
    out = {};
    if (!bytes || size < 4) return false;
    FillMagic(bytes, size, out);

    if (std::memcmp(bytes, "GNF ", 4) == 0) {
        return ClassifyGnf(bytes, size, fileSize, out);
    }
    if (std::memcmp(bytes, "JXMD", 4) == 0) {
        return ClassifyJxmd(bytes, size, fileSize, out);
    }
    if (std::memcmp(bytes, "\x7F" "ELF", 4) == 0) {
        out.kind = AssetKind::Elf64;
        out.valid = size >= 6 && bytes[4] == 2 && bytes[5] == 1;
        return true;
    }
    const uint32_t word = ReadU32(bytes);
    if (word == kSelfMagicLe) {
        out.kind = AssetKind::SelfContainer;
        out.valid = size >= 0x20;
        return true;
    }
    if (word == kSpirvMagicLe) {
        out.kind = AssetKind::Spirv;
        out.valid = size >= 20;
        return true;
    }
    return false;
}

} // namespace AstroPort
