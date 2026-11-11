#include "gameplay/player_movement.h"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace AstroPort;

    const auto near = [](float actual, float expected) {
        return std::abs(actual - expected) < 0.0001f;
    };

    if (!near(AdvancePlayerHorizontalPosition(0.0f, 1.0f, 0.5f),
              kPlayerMoveSpeed * kMaxMovementDeltaTime) ||
        !near(AdvancePlayerHorizontalPosition(0.0f, -1.0f, 0.05f),
              -kPlayerMoveSpeed * 0.05f)) {
        std::cerr << "Horizontal movement direction or speed is incorrect.\n";
        return 1;
    }

    if (!near(AdvancePlayerHorizontalPosition(kPlayerHorizontalLimit, 1.0f, 0.1f),
              kPlayerHorizontalLimit) ||
        !near(AdvancePlayerHorizontalPosition(-kPlayerHorizontalLimit, -1.0f, 0.1f),
              -kPlayerHorizontalLimit)) {
        std::cerr << "Horizontal movement exceeded its bounds.\n";
        return 2;
    }

    if (!near(AdvancePlayerHorizontalPosition(0.5f, 0.0f, 0.1f), 0.5f) ||
        !near(AdvancePlayerHorizontalPosition(0.5f, 1.0f, -1.0f), 0.5f)) {
        std::cerr << "Movement changed with a neutral input or negative delta time.\n";
        return 3;
    }

    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (!near(AdvancePlayerHorizontalPosition(nan, nan, 0.1f), 0.0f) ||
        !near(AdvancePlayerHorizontalPosition(0.0f, 1.0f, nan), 0.0f)) {
        std::cerr << "Movement did not safely handle non-finite input.\n";
        return 4;
    }

    const PlayerPlanarPosition diagonal = AdvancePlayerPlanarPosition(
        {}, 1.0f, 1.0f, 0.1f);
    if (!near(std::sqrt(diagonal.x * diagonal.x + diagonal.z * diagonal.z),
              kPlayerMoveSpeed * kMaxMovementDeltaTime)) {
        std::cerr << "Diagonal movement exceeded the cardinal movement speed.\n";
        return 5;
    }

    const PlayerPlanarPosition bounded = AdvancePlayerPlanarPosition(
        { kPlayerHorizontalLimit, 0.0f }, 1.0f, 1.0f, 0.1f);
    if (!near(std::sqrt(bounded.x * bounded.x + bounded.z * bounded.z),
              kPlayerHorizontalLimit)) {
        std::cerr << "Planar movement exceeded its radial bounds.\n";
        return 6;
    }

    const PlayerPlanarPosition neutral = AdvancePlayerPlanarPosition(
        { 0.2f, -0.3f }, 0.0f, 0.0f, 0.1f);
    if (!near(neutral.x, 0.2f) || !near(neutral.z, -0.3f)) {
        std::cerr << "Neutral planar input changed the position.\n";
        return 7;
    }

    return 0;
}
