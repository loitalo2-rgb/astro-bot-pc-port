#pragma once

#include <cstdint>

namespace AstroPort {

// Geometry of one GNF mip chain. The stored size depends on tiling: each mip is
// packed in 64 KiB tiles, so a mip occupies whole tiles even when the unpadded
// block data is smaller. Both numbers are computed from the header alone.
struct GnfLayout {
    uint64_t unpaddedBytes = 0;  // sum of blockW * blockH * blockBytes
    uint64_t tiledBytes = 0;     // sum of tileCount * 65536 (tile mode != 0)
    uint64_t alignedBytes = 0;   // each mip starts at a 64 KiB boundary
    uint64_t roundedTotalBytes = 0; // contiguous mips, payload rounded to 64 KiB
    uint32_t mipLevels = 0;
    uint32_t tileWidthBlocks = 0;
    uint32_t tileHeightBlocks = 0;
};

// Mirrors the loader's Standard64kBlock: the number of 64 KiB tiles that fit a
// 64 KiB surface for a given block size.
struct GnfTileBlock {
    uint32_t widthBlocks = 64;
    uint32_t heightBlocks = 64;
};

GnfTileBlock GnfStandard64kBlock(uint32_t blockBytes);

GnfLayout ComputeGnfLayout(uint32_t width, uint32_t height,
                           uint32_t mipLevels, uint32_t blockBytes, bool tiled);

} // namespace AstroPort
