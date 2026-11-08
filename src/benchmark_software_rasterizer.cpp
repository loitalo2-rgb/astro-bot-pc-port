#include "common/software_rasterizer.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

int main() {
    using namespace AstroPort;

    constexpr int width = 600;
    constexpr int height = 400;
    constexpr int triangleCount = 136;
    constexpr int warmupFrames = 20;
    constexpr int measuredFrames = 120;
    const size_t pixelCount = static_cast<size_t>(width) * height;

    std::vector<uint32_t> pixels(pixelCount);
    std::vector<float> depthBuffer(pixelCount);
    std::vector<SoftwareRasterVertex> vertices;
    vertices.reserve(triangleCount * 3);
    for (int i = 0; i < triangleCount; ++i) {
        const float phase = static_cast<float>(i) * 2.39996323f;
        const float centerX = static_cast<float>((i * 97) % width);
        const float centerY = static_cast<float>((i * 53) % height);
        const float radiusX = 12.0f + static_cast<float>((i * 29) % 115);
        const float radiusY = 10.0f + static_cast<float>((i * 17) % 90);
        const float depth = 0.15f + static_cast<float>(i % 80) * 0.01f;
        for (int vertex = 0; vertex < 3; ++vertex) {
            const float angle =
                phase + static_cast<float>(vertex) * 2.09439510f;
            vertices.push_back({
                centerX + std::cos(angle) * radiusX,
                centerY + std::sin(angle) * radiusY,
                depth + static_cast<float>(vertex) * 0.002f
            });
        }
    }

    uint64_t writtenPixelCount = 0;
    const auto renderFrame = [&]() {
        std::fill(pixels.begin(), pixels.end(), 0x00160A06u);
        std::fill(depthBuffer.begin(), depthBuffer.end(),
                  -std::numeric_limits<float>::infinity());
        for (int i = 0; i < triangleCount; ++i) {
            const SoftwareRasterVertex triangle[3] = {
                vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]
            };
            writtenPixelCount += RasterizeDepthTriangle(
                pixels, depthBuffer, width, height, triangle,
                static_cast<uint32_t>(i + 1));
        }
    };

    for (int i = 0; i < warmupFrames; ++i) renderFrame();
    writtenPixelCount = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < measuredFrames; ++i) renderFrame();
    const auto end = std::chrono::steady_clock::now();
    const double elapsedSeconds =
        std::chrono::duration<double>(end - start).count();
    const double millisecondsPerFrame =
        elapsedSeconds * 1000.0 / measuredFrames;
    const double framesPerSecond = measuredFrames / elapsedSeconds;
    const double megapixelsPerSecond =
        static_cast<double>(writtenPixelCount) / elapsedSeconds / 1'000'000.0;

    std::cout << "Software rasterizer synthetic benchmark\n"
              << "Viewport: " << width << "x" << height
              << "; triangles/frame: " << triangleCount << '\n'
              << "Warmup: " << warmupFrames << "; measured frames: "
              << measuredFrames << '\n'
              << "Elapsed: " << elapsedSeconds << " s; mean: "
              << millisecondsPerFrame << " ms/frame; equivalent: "
              << framesPerSecond << " frames/s\n"
              << "Depth-passing writes: " << writtenPixelCount
              << "; throughput: " << megapixelsPerSecond << " MPix/s\n";
    return std::isfinite(framesPerSecond) && framesPerSecond > 0.0 ? 0 : 1;
}
