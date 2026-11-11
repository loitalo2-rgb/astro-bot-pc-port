#include "assets/gnf_layout.h"

#include <algorithm>
#include <limits>

namespace AstroPort {

GnfTileBlock GnfStandard64kBlock(uint32_t blockBytes) {
    GnfTileBlock block;
    if (blockBytes == 0) return block;

    const uint32_t elements = 65536u / blockBytes;
    uint32_t bits = 0;
    while ((1u << bits) < elements) ++bits;

    block.widthBlocks = 1u << ((bits + 1) / 2);
    block.heightBlocks = 1u << (bits / 2);
    return block;
}

GnfLayout ComputeGnfLayout(uint32_t width, uint32_t height,
                           uint32_t mipLevels, uint32_t blockBytes, bool tiled) {
    GnfLayout layout;
    if (width == 0 || height == 0 || blockBytes == 0 || mipLevels == 0) {
        return layout;
    }
    if (mipLevels > 32) mipLevels = 32;  // header can only describe 32 levels

    const GnfTileBlock tile = GnfStandard64kBlock(blockBytes);
    layout.tileWidthBlocks = tile.widthBlocks;
    layout.tileHeightBlocks = tile.heightBlocks;
    layout.mipLevels = mipLevels;

    uint64_t alignedOffset = 0;
    for (uint32_t mip = 0; mip < mipLevels; ++mip) {
        const uint32_t shift = mip < 31u ? mip : 31u;
        const uint64_t mipWidth = std::max<uint32_t>(1u, width >> shift);
        const uint64_t mipHeight = std::max<uint32_t>(1u, height >> shift);
        const uint64_t blockW = (mipWidth + 3u) / 4u;
        const uint64_t blockH = (mipHeight + 3u) / 4u;

        if (blockW > std::numeric_limits<uint64_t>::max() / blockH) return layout;
        const uint64_t unpadded = blockW * blockH * blockBytes;
        if (layout.unpaddedBytes >
            std::numeric_limits<uint64_t>::max() - unpadded) {
            return layout;
        }
        layout.unpaddedBytes += unpadded;

        uint64_t stored = unpadded;
        if (tiled) {
            const uint64_t tilesX =
                (blockW + tile.widthBlocks - 1u) / tile.widthBlocks;
            const uint64_t tilesY =
                (blockH + tile.heightBlocks - 1u) / tile.heightBlocks;
            stored = tilesX * tilesY * 65536u;
        }
        if (layout.tiledBytes > std::numeric_limits<uint64_t>::max() - stored) {
            return layout;
        }
        layout.tiledBytes += stored;

        // Alternative hypothesis: mips are laid back to back, but each one
        // starts at a 64 KiB boundary (padding only between mips).
        if (mip != 0 && tiled) {
            alignedOffset = (alignedOffset + 65535u) / 65536u * 65536u;
        }
        alignedOffset += unpadded;
        layout.alignedBytes = alignedOffset;
    }
    if (tiled) {
        // Whole-payload hypothesis: mips are contiguous and the payload is
        // rounded up to a 64 KiB boundary as a single block.
        layout.roundedTotalBytes =
            (layout.unpaddedBytes + 65535u) / 65536u * 65536u;
    } else {
        layout.roundedTotalBytes = layout.unpaddedBytes;
    }
    return layout;
}

} // namespace AstroPort
