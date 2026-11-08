#include "common/software_rasterizer.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

int main() {
    using namespace AstroPort;

    constexpr int width = 8;
    constexpr int height = 8;
    constexpr uint32_t background = 0x00000011u;
    constexpr uint32_t backColor = 0x00000022u;
    constexpr uint32_t frontColor = 0x00000033u;
    std::vector<uint32_t> pixels(width * height, background);
    std::vector<float> depth(width * height,
                             -std::numeric_limits<float>::infinity());

    const SoftwareRasterVertex varyingDepthTriangle[3] = {
        { 0.0f, 0.0f, 0.1f },
        { 8.0f, 0.0f, 0.9f },
        { 0.0f, 8.0f, 0.5f }
    };
    if (RasterizeDepthTriangle(pixels, depth, width, height,
                               varyingDepthTriangle, backColor) == 0) {
        std::cerr << "The visible triangle wrote no pixels.\n";
        return 1;
    }
    if (pixels[0] != backColor || std::abs(depth[0] - 0.175f) > 1e-5f) {
        std::cerr << "Depth was not interpolated from the matching vertices.\n";
        return 2;
    }

    const SoftwareRasterVertex frontTriangle[3] = {
        { 1.0f, 1.0f, 0.34f },
        { 4.0f, 1.0f, 0.34f },
        { 1.0f, 4.0f, 0.34f }
    };
    if (RasterizeDepthTriangle(pixels, depth, width, height,
                               frontTriangle, frontColor) == 0) {
        std::cerr << "The nearer triangle wrote no pixels.\n";
        return 3;
    }

    const size_t overlapPixel = 1u * width + 1u;
    if (pixels[overlapPixel] != frontColor ||
        std::abs(depth[overlapPixel] - 0.34f) > 1e-5f) {
        std::cerr << "Depth interpolation or the nearer-surface test is incorrect.\n";
        return 4;
    }

    const SoftwareRasterVertex degenerateTriangle[3] = {
        { 0.0f, 0.0f, 1.0f },
        { 2.0f, 2.0f, 1.0f },
        { 4.0f, 4.0f, 1.0f }
    };
    if (RasterizeDepthTriangle(pixels, depth, width, height,
                               degenerateTriangle, 0x44u) != 0 ||
        pixels[overlapPixel] != frontColor) {
        std::cerr << "The degenerate triangle was not rejected.\n";
        return 5;
    }

    std::vector<uint32_t> undersizedPixels(1, background);
    std::vector<float> undersizedDepth(1, 0.0f);
    if (RasterizeDepthTriangle(undersizedPixels, undersizedDepth,
                               width, height, frontTriangle, frontColor) != 0) {
        std::cerr << "Rasterization accepted undersized buffers.\n";
        return 6;
    }

    const SoftwareRasterVertex reversedTriangle[3] = {
        { 7.0f, 7.0f, 0.5f },
        { 7.0f, 5.0f, 0.5f },
        { 5.0f, 7.0f, 0.5f }
    };
    if (RasterizeDepthTriangle(pixels, depth, width, height,
                               reversedTriangle, 0x55u) == 0 ||
        pixels[6u * width + 6u] != 0x55u) {
        std::cerr << "The reversed-winding triangle was not rasterized.\n";
        return 7;
    }

    std::cout << "Software depth rasterizer validation passed.\n";
    return 0;
}
