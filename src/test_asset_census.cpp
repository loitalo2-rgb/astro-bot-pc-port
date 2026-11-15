// Asset header classification tests (offline census; nothing is executed).
//
// Builds synthetic headers that mirror the real dump (GNF v4 with a BC4
// descriptor, JXMD with a valid first chunk) and verifies both positive
// classification and the "magic known but header invalid" cases.

#include "assets/asset_census.h"
#include "assets/gnf_layout.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void WriteLe(std::vector<uint8_t>& bytes, size_t offset, uint64_t value,
             size_t width) {
    for (size_t i = 0; i < width; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
    }
}

std::vector<uint8_t> MakeGnf(uint32_t formatId, uint32_t widthBlocks,
                             uint32_t height, uint32_t mipLevels,
                             uint32_t tileMode, uint32_t payloadSize,
                             bool truncateHeader = false) {
    std::vector<uint8_t> bytes(truncateHeader ? 8 : 0x80, 0);
    std::memcpy(bytes.data(), "GNF ", 4);
    WriteLe(bytes, 4, 0x48, 4);   // contentsSize -> dataOffset 0x50
    bytes[8] = 4;                 // version 4 (descriptor at 0x10)
    bytes[9] = 1;                 // one texture
    bytes[10] = 0x0C;
    if (truncateHeader) return bytes;

    const size_t descriptor = 0x10;
    WriteLe(bytes, descriptor + 0x04, (uint64_t(formatId) << 16) | 0x000Cu, 4);
    WriteLe(bytes, descriptor + 0x08,
            (uint64_t(widthBlocks - 1) & 0x3FFFu) |
                ((uint64_t(height - 1) & 0x3FFFu) << 14),
            4);
    WriteLe(bytes, descriptor + 0x0C,
            ((uint64_t(mipLevels - 1) & 0xFu) << 16) |
                ((uint64_t(tileMode) & 0x1Fu) << 20),
            4);
    WriteLe(bytes, descriptor + 0x1C, payloadSize, 4);
    return bytes;
}

std::vector<uint8_t> MakeJxmd(uint32_t chunkSize, uint32_t count,
                              uint32_t stride) {
    std::vector<uint8_t> bytes(0x40, 0);
    std::memcpy(bytes.data(), "JXMD", 4);
    WriteLe(bytes, 4, chunkSize, 4);
    WriteLe(bytes, 12, 0x20, 4);   // payload offset
    WriteLe(bytes, 16, 0xA76CABu, 4);
    WriteLe(bytes, 20, count, 4);
    WriteLe(bytes, 24, stride, 4);
    WriteLe(bytes, 28, 1, 4);
    return bytes;
}

} // namespace

int main() {
    size_t failures = 0;
    const auto expect = [&](bool condition, const char* name) {
        if (!condition) {
            std::cerr << "Asset census test failed: " << name << "\n";
            ++failures;
        }
    };

    // --------------------------------------------------------------
    // GNF: real-dump shape (BC4, 4x4 blocks, 4 mips, tile mode 4).
    // --------------------------------------------------------------
    {
        auto bytes = MakeGnf(0xCAF0u, 1, 256, 4, 4, 0x8000);
        AstroPort::AssetHeaderInfo info;
        const bool recognized = AstroPort::ClassifyAssetHeader(
            bytes.data(), bytes.size(), 0x9000, info);
        expect(recognized, "GNF magic recognized");
        expect(info.kind == AstroPort::AssetKind::Gnf, "GNF kind");
        expect(info.valid, "GNF header valid");
        expect(info.gnfVersion == 4 && info.gnfTextures == 1,
               "GNF version and texture count");
        expect(info.formatId == 0xCAF0u, "GNF format id");
        expect(info.width == 4 && info.height == 256, "GNF dimensions");
        expect(info.mipLevels == 4, "GNF mip count");
        expect(info.tileMode == 4, "GNF tile mode");
        expect(info.bytesPerBlock == 8, "BC4 is 8 bytes per block");
        expect(info.payloadSize == 0x8000, "GNF payload size");
        expect(info.dataOffset == 0x50, "GNF data offset derived from header");
        expect(info.gnfFormatKnown, "BC4 format id is mapped");
        expect(info.gnfFormatName.find("BC4") != std::string::npos,
               "BC4 format name");
    }

    // --------------------------------------------------------------
    // GNF BC7: 16 bytes per block, 128x64.
    // --------------------------------------------------------------
    {
        auto bytes = MakeGnf(0xCB30u, 32, 64, 1, 0, 0x1000);
        AstroPort::AssetHeaderInfo info;
        expect(AstroPort::ClassifyAssetHeader(bytes.data(), bytes.size(),
                                              0x2000, info),
               "BC7 GNF recognized");
        expect(info.valid && info.bytesPerBlock == 16,
               "BC7 is 16 bytes per block");
        expect(info.width == 128 && info.height == 64, "BC7 dimensions");
        expect(info.mipLevels == 1, "single mip level");
        expect(info.gnfFormatKnown && info.gnfFormatName.find("BC7") != std::string::npos,
               "BC7 sRGB format mapped");

        auto normal = MakeGnf(0xCB10u, 1, 1, 1, 9, 0x10);
        expect(AstroPort::ClassifyAssetHeader(normal.data(), normal.size(),
                                              0x100, info),
               "BC7 UNORM variant recognized");
        expect(info.gnfFormatKnown && info.gnfFormatName.find("UNORM") != std::string::npos,
               "BC7 UNORM variant mapped");

        auto unknown = MakeGnf(0xC380u, 1, 1, 1, 9, 0x10);
        expect(AstroPort::ClassifyAssetHeader(unknown.data(), unknown.size(),
                                              0x100, info),
               "unmapped GNF format still recognized");
        expect(info.valid && !info.gnfFormatKnown,
               "unmapped GNF format flagged as unknown");
    }

    // --------------------------------------------------------------
    // GNF truncated header / data offset past EOF => magic only.
    // --------------------------------------------------------------
    {
        auto truncated = MakeGnf(0xCAF0u, 1, 1, 1, 0, 0, true);
        AstroPort::AssetHeaderInfo info;
        expect(AstroPort::ClassifyAssetHeader(truncated.data(), truncated.size(),
                                              0x100, info),
               "truncated GNF still recognized by magic");
        expect(info.kind == AstroPort::AssetKind::Gnf && !info.valid,
               "truncated GNF reported invalid");

        auto complete = MakeGnf(0xCAF0u, 1, 1, 1, 0, 0x10);
        expect(AstroPort::ClassifyAssetHeader(complete.data(), complete.size(),
                                              0x10, info),
               "GNF with dataOffset past EOF recognized");
        expect(!info.valid, "GNF with dataOffset past EOF invalid");
    }

    // --------------------------------------------------------------
    // JXMD: valid first chunk and oversized chunk (invalid).
    // --------------------------------------------------------------
    {
        auto bytes = MakeJxmd(0x110, 12, 0x24);
        AstroPort::AssetHeaderInfo info;
        expect(AstroPort::ClassifyAssetHeader(bytes.data(), bytes.size(),
                                              0x400, info),
               "JXMD magic recognized");
        expect(info.kind == AstroPort::AssetKind::Jxmd && info.valid,
               "JXMD header valid");
        expect(info.magic == "JXMD", "JXMD magic text");
        expect(info.chunkSize == 0x110 && info.chunkCount == 12 &&
                   info.chunkStride == 0x24,
               "JXMD chunk fields");

        auto oversized = MakeJxmd(0x1000, 12, 0x24);
        expect(AstroPort::ClassifyAssetHeader(oversized.data(), oversized.size(),
                                              0x400, info),
               "oversized JXMD recognized");
        expect(!info.valid, "JXMD chunk past EOF reported invalid");
    }

    // --------------------------------------------------------------
    // Loader/container magics and unknown data.
    // --------------------------------------------------------------
    {
        std::vector<uint8_t> elf = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0};
        AstroPort::AssetHeaderInfo info;
        expect(AstroPort::ClassifyAssetHeader(elf.data(), elf.size(), 0x100, info),
               "ELF64 recognized");
        expect(info.kind == AstroPort::AssetKind::Elf64 && info.valid,
               "ELF64 valid");

        std::vector<uint8_t> self = {0x4F, 0x15, 0x3D, 0x1D};
        self.resize(0x40);
        expect(AstroPort::ClassifyAssetHeader(self.data(), self.size(), 0x1000,
                                              info),
               "SELF recognized");
        expect(info.kind == AstroPort::AssetKind::SelfContainer && info.valid,
               "SELF valid");

        std::vector<uint8_t> spirv = {0x03, 0x02, 0x23, 0x07};
        spirv.resize(0x40);
        expect(AstroPort::ClassifyAssetHeader(spirv.data(), spirv.size(), 0x100,
                                              info),
               "SPIR-V recognized");
        expect(info.kind == AstroPort::AssetKind::Spirv && info.valid,
               "SPIR-V valid");

        std::vector<uint8_t> random = {0x38, 0x30, 0x41, 0x45, 0, 1, 2, 3};
        expect(!AstroPort::ClassifyAssetHeader(random.data(), random.size(),
                                               0x100, info),
               "animation tag is not an asset container");
        expect(!AstroPort::ClassifyAssetHeader(random.data(), 2, 0x100, info),
               "short header rejected");
        expect(!AstroPort::ClassifyAssetHeader(nullptr, 8, 0x100, info),
               "null header rejected");
    }

    // --------------------------------------------------------------
    // GNF layout: mip chain and 64 KiB tile packing.
    // --------------------------------------------------------------
    {
        const auto bc4Tile = AstroPort::GnfStandard64kBlock(8);
        expect(bc4Tile.widthBlocks == 128 && bc4Tile.heightBlocks == 64,
               "BC4 64 KiB tile is 128x64 blocks");
        const auto bc7Tile = AstroPort::GnfStandard64kBlock(16);
        expect(bc7Tile.widthBlocks == 64 && bc7Tile.heightBlocks == 64,
               "BC7 64 KiB tile is 64x64 blocks");

        // A 4x4 BC4 texture with one mip still occupies one whole 64 KiB tile.
        const auto tiny =
            AstroPort::ComputeGnfLayout(4, 4, 1, 8, true);
        expect(tiny.unpaddedBytes == 8, "4x4 BC4 unpadded is 8 bytes");
        expect(tiny.tiledBytes == 65536, "4x4 BC4 occupies one 64 KiB tile");

        // 128x64 BC7: 32x16 blocks = 8192 bytes, still one tile.
        const auto small =
            AstroPort::ComputeGnfLayout(128, 64, 1, 16, true);
        expect(small.unpaddedBytes == 8192, "128x64 BC7 unpadded is 8192 bytes");
        expect(small.tiledBytes == 65536, "128x64 BC7 occupies one tile");

        // 4096x4096 BC7 level 0: 1024x1024 blocks = 16 MiB, exactly 256 tiles.
        const auto big =
            AstroPort::ComputeGnfLayout(4096, 4096, 1, 16, true);
        expect(big.unpaddedBytes == 16ull * 1024 * 1024,
               "4096x4096 BC7 unpadded is 16 MiB");
        expect(big.tiledBytes == big.unpaddedBytes,
               "aligned large mip tiles exactly");

        // Untiled layout ignores tile packing entirely.
        const auto linear =
            AstroPort::ComputeGnfLayout(128, 64, 2, 16, false);
        expect(linear.tiledBytes == linear.unpaddedBytes,
               "linear layout has no tile padding");
        expect(linear.mipLevels == 2, "mip count preserved");
        expect(linear.alignedBytes == linear.unpaddedBytes,
               "linear layout has no 64 KiB alignment padding");

        const auto aligned = AstroPort::ComputeGnfLayout(128, 64, 2, 16, true);
        expect(aligned.alignedBytes == 65536ull + 2048ull,
               "second tiled mip starts at a 64 KiB boundary");
        expect(aligned.roundedTotalBytes == 65536ull,
               "whole payload rounded to one 64 KiB block");

        const auto degenerate =
            AstroPort::ComputeGnfLayout(0, 64, 4, 16, true);
        expect(degenerate.unpaddedBytes == 0 && degenerate.tiledBytes == 0,
               "zero-sized layout is empty");
    }

    if (failures != 0) {
        std::cerr << failures << " asset census test(s) failed.\n";
        return 1;
    }
    std::cout << "Asset census validation passed.\n";
    return 0;
}
