#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace AstroPort {

// Offline census of the game's asset containers. Classification reads only the
// fixed headers already understood by the project (GNF from asset_loader, JXMD
// from jxmd_parser) plus the container magics used by the loader. Nothing is
// decoded, decompressed or executed.
enum class AssetKind {
    Unknown = 0,
    Gnf,            // "GNF " texture container (PS4/PS5)
    Jxmd,           // "JXMD" mesh container
    SelfContainer,  // SELF (0x1D3D154F)
    Elf64,
    Spirv,
};

struct AssetHeaderInfo {
    AssetKind kind = AssetKind::Unknown;
    std::string magic;      // first four bytes as text (printer-friendly)
    bool valid = false;     // header internally consistent and inside the file

    // GNF
    uint32_t gnfVersion = 0;
    uint32_t gnfTextures = 0;
    uint32_t gnfContentsSize = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t formatId = 0;
    uint32_t tileMode = 0;
    uint32_t mipLevels = 0;
    uint32_t dataOffset = 0;
    uint32_t payloadSize = 0;
    uint32_t bytesPerBlock = 0;
    bool gnfFormatKnown = false;
    std::string gnfFormatName = "desconhecido";

    // JXMD (first chunk header)
    uint32_t chunkSize = 0;
    uint32_t chunkPayloadOffset = 0;
    uint32_t chunkCount = 0;
    uint32_t chunkStride = 0;
    uint32_t chunkFlags = 0;
};

// `bytes` is the head of the file (at least 4 bytes; ideally 96). `fileSize`
// is the real size on disk and is used to reject headers that point past the
// end. Returns true when the magic matches a known container (even if the
// header is truncated/inconsistent: check `valid` for that case).
bool ClassifyAssetHeader(const uint8_t* bytes, size_t size, uint64_t fileSize,
                         AssetHeaderInfo& out);

} // namespace AstroPort
