#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace AstroPort {

struct SoftwareRasterVertex {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
};

inline size_t RasterizeDepthTriangle(
    std::vector<uint32_t>& pixels, std::vector<float>& depthBuffer,
    int width, int height, const SoftwareRasterVertex (&vertices)[3],
    uint32_t color) {
    if (width <= 0 || height <= 0) return 0;
    const size_t pixelCount = static_cast<size_t>(width) *
                              static_cast<size_t>(height);
    if (pixels.size() < pixelCount || depthBuffer.size() < pixelCount) {
        return 0;
    }

    const SoftwareRasterVertex& a = vertices[0];
    const SoftwareRasterVertex& b = vertices[1];
    const SoftwareRasterVertex& c = vertices[2];
    if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
        !std::isfinite(a.depth) || !std::isfinite(b.x) ||
        !std::isfinite(b.y) || !std::isfinite(b.depth) ||
        !std::isfinite(c.x) || !std::isfinite(c.y) ||
        !std::isfinite(c.depth)) {
        return 0;
    }
    const float area =
        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (!std::isfinite(area) || std::abs(area) <= 1e-6f) return 0;

    const float minVertexX = std::min(a.x, std::min(b.x, c.x));
    const float maxVertexX = std::max(a.x, std::max(b.x, c.x));
    const float minVertexY = std::min(a.y, std::min(b.y, c.y));
    const float maxVertexY = std::max(a.y, std::max(b.y, c.y));
    if (maxVertexX < 0.0f || maxVertexY < 0.0f ||
        minVertexX > static_cast<float>(width - 1) ||
        minVertexY > static_cast<float>(height - 1)) {
        return 0;
    }
    const int minX = std::clamp(
        static_cast<int>(std::floor(std::clamp(minVertexX, 0.0f,
                                               static_cast<float>(width - 1)))),
        0, width - 1);
    const int maxX = std::clamp(
        static_cast<int>(std::ceil(std::clamp(maxVertexX, 0.0f,
                                              static_cast<float>(width - 1)))),
        0, width - 1);
    const int minY = std::clamp(
        static_cast<int>(std::floor(std::clamp(minVertexY, 0.0f,
                                               static_cast<float>(height - 1)))),
        0, height - 1);
    const int maxY = std::clamp(
        static_cast<int>(std::ceil(std::clamp(maxVertexY, 0.0f,
                                              static_cast<float>(height - 1)))),
        0, height - 1);
    if (minX > maxX || minY > maxY) return 0;

    const float inverseArea = 1.0f / area;
    const float dw0dx = (b.y - c.y) * inverseArea;
    const float dw0dy = (c.x - b.x) * inverseArea;
    const float dw1dx = (c.y - a.y) * inverseArea;
    const float dw1dy = (a.x - c.x) * inverseArea;
    const float dw2dx = -dw0dx - dw1dx;
    const float dw2dy = -dw0dy - dw1dy;
    const float depthDx =
        dw0dx * a.depth + dw1dx * b.depth + dw2dx * c.depth;
    const float depthDy =
        dw0dy * a.depth + dw1dy * b.depth + dw2dy * c.depth;
    constexpr float edgeTolerance = -1e-5f;
    const float firstSampleX = static_cast<float>(minX) + 0.5f;
    const float firstSampleY = static_cast<float>(minY) + 0.5f;
    float rowW0 =
        ((b.x - firstSampleX) * (c.y - firstSampleY) -
         (b.y - firstSampleY) * (c.x - firstSampleX)) * inverseArea;
    float rowW1 =
        ((c.x - firstSampleX) * (a.y - firstSampleY) -
         (c.y - firstSampleY) * (a.x - firstSampleX)) * inverseArea;
    float rowW2 = 1.0f - rowW0 - rowW1;
    float rowDepth = rowW0 * a.depth + rowW1 * b.depth + rowW2 * c.depth;
    size_t writtenPixels = 0;

    for (int y = minY; y <= maxY; ++y) {
        float w0 = rowW0;
        float w1 = rowW1;
        float w2 = rowW2;
        float fragmentDepth = rowDepth;
        size_t pixelIndex =
            static_cast<size_t>(y) * static_cast<size_t>(width) +
            static_cast<size_t>(minX);

        for (int x = minX; x <= maxX; ++x) {
            if (w0 >= edgeTolerance && w1 >= edgeTolerance &&
                w2 >= edgeTolerance && fragmentDepth > depthBuffer[pixelIndex]) {
                depthBuffer[pixelIndex] = fragmentDepth;
                pixels[pixelIndex] = color;
                ++writtenPixels;
            }
            w0 += dw0dx;
            w1 += dw1dx;
            w2 += dw2dx;
            fragmentDepth += depthDx;
            ++pixelIndex;
        }
        rowW0 += dw0dy;
        rowW1 += dw1dy;
        rowW2 += dw2dy;
        rowDepth += depthDy;
    }
    return writtenPixels;
}

} // namespace AstroPort
