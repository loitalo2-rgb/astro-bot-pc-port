#pragma once

#include <algorithm>
#include <cmath>

namespace AstroPort {

inline constexpr float kPlayerMoveSpeed = 1.8f;
inline constexpr float kPlayerHorizontalLimit = 1.2f;
inline constexpr float kMaxMovementDeltaTime = 0.1f;

struct PlayerPlanarPosition {
    float x = 0.0f;
    float z = 0.0f;
};

inline float AdvancePlayerHorizontalPosition(float position, float stickX,
                                             float deltaTime) {
    if (!std::isfinite(position)) position = 0.0f;
    if (!std::isfinite(stickX)) stickX = 0.0f;
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f) deltaTime = 0.0f;

    position = std::clamp(position, -kPlayerHorizontalLimit,
                          kPlayerHorizontalLimit);
    stickX = std::clamp(stickX, -1.0f, 1.0f);
    deltaTime = std::min(deltaTime, kMaxMovementDeltaTime);

    return std::clamp(position + stickX * kPlayerMoveSpeed * deltaTime,
                      -kPlayerHorizontalLimit, kPlayerHorizontalLimit);
}

inline PlayerPlanarPosition AdvancePlayerPlanarPosition(
    PlayerPlanarPosition position, float stickX, float stickY, float deltaTime) {
    if (!std::isfinite(position.x)) position.x = 0.0f;
    if (!std::isfinite(position.z)) position.z = 0.0f;
    if (!std::isfinite(stickX)) stickX = 0.0f;
    if (!std::isfinite(stickY)) stickY = 0.0f;
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f) deltaTime = 0.0f;

    position.x = std::clamp(position.x, -kPlayerHorizontalLimit,
                            kPlayerHorizontalLimit);
    position.z = std::clamp(position.z, -kPlayerHorizontalLimit,
                            kPlayerHorizontalLimit);
    stickX = std::clamp(stickX, -1.0f, 1.0f);
    stickY = std::clamp(stickY, -1.0f, 1.0f);
    const float magnitude = std::sqrt(stickX * stickX + stickY * stickY);
    if (magnitude > 1.0f) {
        stickX /= magnitude;
        stickY /= magnitude;
    }

    deltaTime = std::min(deltaTime, kMaxMovementDeltaTime);
    const float distance = kPlayerMoveSpeed * deltaTime;
    position.x += stickX * distance;
    position.z += stickY * distance;

    const float positionMagnitude =
        std::sqrt(position.x * position.x + position.z * position.z);
    if (positionMagnitude > kPlayerHorizontalLimit) {
        position.x = position.x / positionMagnitude * kPlayerHorizontalLimit;
        position.z = position.z / positionMagnitude * kPlayerHorizontalLimit;
    }
    return position;
}

} // namespace AstroPort
