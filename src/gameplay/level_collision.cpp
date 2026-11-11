#include "level_collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace AstroPort {
namespace {

bool IsFinite(const JxmdVertex& vertex) {
    return std::isfinite(vertex.x) && std::isfinite(vertex.y) &&
           std::isfinite(vertex.z);
}

float PointSegmentDistanceSquared(float x, float z,
                                  const JxmdVertex& a,
                                  const JxmdVertex& b) {
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    const float lengthSquared = dx * dx + dz * dz;
    const float amount = lengthSquared > 1e-8f
        ? std::clamp(((x - a.x) * dx + (z - a.z) * dz) / lengthSquared,
                     0.0f, 1.0f)
        : 0.0f;
    const float closestX = a.x + amount * dx;
    const float closestZ = a.z + amount * dz;
    const float offsetX = x - closestX;
    const float offsetZ = z - closestZ;
    return offsetX * offsetX + offsetZ * offsetZ;
}

bool IsWithinTriangleProjection(float x, float z,
                                const JxmdVertex& a,
                                const JxmdVertex& b,
                                const JxmdVertex& c) {
    const float denominator =
        (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
    if (std::abs(denominator) <= 1e-8f) return false;

    const float wa =
        ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) /
        denominator;
    const float wb =
        ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) /
        denominator;
    const float wc = 1.0f - wa - wb;
    constexpr float edgeTolerance = 1e-5f;
    return wa >= -edgeTolerance && wb >= -edgeTolerance &&
           wc >= -edgeTolerance;
}

} // namespace

bool LevelCollisionMesh::Build(const JxmdMesh& mesh, float minimumUpNormal) {
    m_triangles.clear();
    m_obstacles.clear();
    m_minX = m_minZ = std::numeric_limits<float>::infinity();
    m_maxX = m_maxZ = -std::numeric_limits<float>::infinity();
    if (!mesh.valid || !mesh.collisionDataValid) return false;
    const std::vector<uint32_t>& indices = mesh.collisionIndicesRestricted
        ? mesh.collisionIndices
        : mesh.indices;
    if (indices.size() < 3) return false;
    if (!std::isfinite(minimumUpNormal)) minimumUpNormal = 0.65f;
    minimumUpNormal = std::clamp(minimumUpNormal, 0.0f, 1.0f);

    m_triangles.reserve(indices.size() / 3);
    m_obstacles.reserve(indices.size() / 3);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t ia = indices[i];
        const uint32_t ib = indices[i + 1];
        const uint32_t ic = indices[i + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() ||
            ic >= mesh.vertices.size()) {
            continue;
        }

        const JxmdVertex& a = mesh.vertices[ia];
        const JxmdVertex& b = mesh.vertices[ib];
        const JxmdVertex& c = mesh.vertices[ic];
        if (!IsFinite(a) || !IsFinite(b) || !IsFinite(c)) continue;

        const float e1x = b.x - a.x;
        const float e1y = b.y - a.y;
        const float e1z = b.z - a.z;
        const float e2x = c.x - a.x;
        const float e2y = c.y - a.y;
        const float e2z = c.z - a.z;
        const float normalX = e1y * e2z - e1z * e2y;
        const float normalY = e1z * e2x - e1x * e2z;
        const float normalZ = e1x * e2y - e1y * e2x;
        const float normalLength = std::sqrt(
            normalX * normalX + normalY * normalY + normalZ * normalZ);
        if (!std::isfinite(normalLength) || normalLength <= 1e-8f) continue;

        if (std::abs(normalY) / normalLength >= minimumUpNormal) {
            m_triangles.push_back({ a, b, c });
            for (const JxmdVertex* vertex : { &a, &b, &c }) {
                m_minX = std::min(m_minX, vertex->x);
                m_maxX = std::max(m_maxX, vertex->x);
                m_minZ = std::min(m_minZ, vertex->z);
                m_maxZ = std::max(m_maxZ, vertex->z);
            }
        } else {
            m_obstacles.push_back({ a, b, c });
        }
    }

    return !m_triangles.empty();
}

bool LevelCollisionMesh::FindGroundHeight(float x, float z, float& height) const {
    if (!std::isfinite(x) || !std::isfinite(z)) return false;

    bool found = false;
    float highest = -std::numeric_limits<float>::infinity();
    for (const Triangle& triangle : m_triangles) {
        const JxmdVertex& a = triangle.a;
        const JxmdVertex& b = triangle.b;
        const JxmdVertex& c = triangle.c;
        const float denominator =
            (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
        if (std::abs(denominator) <= 1e-8f) continue;

        const float wa =
            ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) /
            denominator;
        const float wb =
            ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) /
            denominator;
        const float wc = 1.0f - wa - wb;
        constexpr float edgeTolerance = 1e-5f;
        if (wa < -edgeTolerance || wb < -edgeTolerance || wc < -edgeTolerance) {
            continue;
        }

        const float candidateHeight = wa * a.y + wb * b.y + wc * c.y;
        if (std::isfinite(candidateHeight) && (!found || candidateHeight > highest)) {
            highest = candidateHeight;
            found = true;
        }
    }

    if (found) height = highest;
    return found;
}

bool LevelCollisionMesh::FindGroundHeightNear(
    float x, float z, float referenceHeight, float maxStepUp,
    float maxStepDown, float& height) const {
    if (!std::isfinite(x) || !std::isfinite(z) ||
        !std::isfinite(referenceHeight) || !std::isfinite(maxStepUp) ||
        !std::isfinite(maxStepDown) || maxStepUp < 0.0f || maxStepDown < 0.0f) {
        return false;
    }

    bool found = false;
    float closestHeight = -std::numeric_limits<float>::infinity();
    const float minimumHeight = referenceHeight - maxStepDown;
    const float maximumHeight = referenceHeight + maxStepUp;
    for (const Triangle& triangle : m_triangles) {
        const JxmdVertex& a = triangle.a;
        const JxmdVertex& b = triangle.b;
        const JxmdVertex& c = triangle.c;
        const float denominator =
            (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
        if (std::abs(denominator) <= 1e-8f) continue;

        const float wa =
            ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) /
            denominator;
        const float wb =
            ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) /
            denominator;
        const float wc = 1.0f - wa - wb;
        constexpr float edgeTolerance = 1e-5f;
        if (wa < -edgeTolerance || wb < -edgeTolerance || wc < -edgeTolerance) {
            continue;
        }

        const float candidateHeight = wa * a.y + wb * b.y + wc * c.y;
        if (std::isfinite(candidateHeight) && candidateHeight >= minimumHeight &&
            candidateHeight <= maximumHeight &&
            (!found || candidateHeight > closestHeight)) {
            closestHeight = candidateHeight;
            found = true;
        }
    }

    if (found) height = closestHeight;
    return found;
}

bool LevelCollisionMesh::FindGroundHeightForFootprint(
    float x, float z, float referenceHeight, float maxStepUp,
    float maxStepDown, float supportRadius, float& height) const {
    if (!std::isfinite(supportRadius) || supportRadius < 0.0f) return false;

    float centerHeight = 0.0f;
    if (!FindGroundHeightNear(x, z, referenceHeight, maxStepUp,
                              maxStepDown, centerHeight)) {
        return false;
    }

    if (supportRadius == 0.0f) {
        height = centerHeight;
        return true;
    }

    constexpr float diagonalScale = 0.70710678118f;
    const float diagonalRadius = supportRadius * diagonalScale;
    const float sampleOffsets[8][2] = {
        { -supportRadius, 0.0f },
        { supportRadius, 0.0f },
        { 0.0f, -supportRadius },
        { 0.0f, supportRadius },
        { -diagonalRadius, -diagonalRadius },
        { -diagonalRadius, diagonalRadius },
        { diagonalRadius, -diagonalRadius },
        { diagonalRadius, diagonalRadius }
    };
    for (const auto& offset : sampleOffsets) {
        float supportHeight = 0.0f;
        if (!FindGroundHeightNear(x + offset[0], z + offset[1],
                                  centerHeight, maxStepUp, maxStepDown,
                                  supportHeight)) {
            return false;
        }
    }

    height = centerHeight;
    return true;
}

bool LevelCollisionMesh::IsBodyBlocked(float x, float z, float groundHeight,
                                       float supportRadius,
                                       float bodyHeight) const {
    const float minimumBodyY = groundHeight;
    const float maximumBodyY = groundHeight + bodyHeight;
    const float radiusSquared = supportRadius * supportRadius;
    for (const Triangle& obstacle : m_obstacles) {
        const float obstacleMinY = std::min(
            obstacle.a.y, std::min(obstacle.b.y, obstacle.c.y));
        const float obstacleMaxY = std::max(
            obstacle.a.y, std::max(obstacle.b.y, obstacle.c.y));
        if (obstacleMaxY < minimumBodyY || obstacleMinY > maximumBodyY) {
            continue;
        }

        if (IsWithinTriangleProjection(x, z, obstacle.a, obstacle.b,
                                       obstacle.c)) {
            return true;
        }
        const float nearestEdgeDistance = std::min(
            PointSegmentDistanceSquared(x, z, obstacle.a, obstacle.b),
            std::min(
                PointSegmentDistanceSquared(x, z, obstacle.b, obstacle.c),
                PointSegmentDistanceSquared(x, z, obstacle.c, obstacle.a)));
        if (nearestEdgeDistance <= radiusSquared) return true;
    }
    return false;
}

bool LevelCollisionMesh::MoveOnGround(
    float fromX, float fromZ, float toX, float toZ, float startingHeight,
    float maxStepUp, float maxStepDown, float& finalHeight,
    float supportRadius, float bodyHeight) const {
    if (!std::isfinite(fromX) || !std::isfinite(fromZ) ||
        !std::isfinite(toX) || !std::isfinite(toZ) ||
        !std::isfinite(startingHeight) || !std::isfinite(maxStepUp) ||
        !std::isfinite(maxStepDown) || !std::isfinite(supportRadius) ||
        !std::isfinite(bodyHeight) || maxStepUp < 0.0f ||
        maxStepDown < 0.0f || supportRadius < 0.0f || bodyHeight <= 0.0f) {
        return false;
    }

    const float dx = toX - fromX;
    const float dz = toZ - fromZ;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (!std::isfinite(distance)) return false;
    const size_t sampleCount = static_cast<size_t>(
        std::max(1.0f, std::ceil(distance / 0.1f)));
    if (sampleCount > 256) return false;

    float previousHeight = startingHeight;
    for (size_t sample = 0; sample <= sampleCount; ++sample) {
        const float amount = static_cast<float>(sample) /
                             static_cast<float>(sampleCount);
        const float sampleX = fromX + dx * amount;
        const float sampleZ = fromZ + dz * amount;
        float sampleHeight = 0.0f;
        if (!FindGroundHeightForFootprint(sampleX, sampleZ, previousHeight,
                                          maxStepUp, maxStepDown,
                                          supportRadius, sampleHeight)) {
            return false;
        }
        if (IsBodyBlocked(sampleX, sampleZ, sampleHeight,
                          supportRadius, bodyHeight)) {
            return false;
        }
        previousHeight = sampleHeight;
    }

    finalHeight = previousHeight;
    return true;
}

bool LevelCollisionMesh::MoveOnGroundWithSlide(
    float fromX, float fromZ, float toX, float toZ, float startingHeight,
    float maxStepUp, float maxStepDown, float& finalX, float& finalZ,
    float& finalHeight, float supportRadius, float bodyHeight) const {
    float directHeight = startingHeight;
    if (MoveOnGround(fromX, fromZ, toX, toZ, startingHeight,
                     maxStepUp, maxStepDown, directHeight, supportRadius,
                     bodyHeight)) {
        finalX = toX;
        finalZ = toZ;
        finalHeight = directHeight;
        return true;
    }

    struct CandidateMove {
        float x;
        float z;
        float height;
        float progressSquared;
    };
    CandidateMove best{ fromX, fromZ, startingHeight, 0.0f };
    bool found = false;
    const float candidatePositions[2][2] = {
        { toX, fromZ },
        { fromX, toZ }
    };
    for (const auto& position : candidatePositions) {
        float candidateHeight = startingHeight;
        if (!MoveOnGround(fromX, fromZ, position[0], position[1],
                          startingHeight, maxStepUp, maxStepDown,
                          candidateHeight, supportRadius, bodyHeight)) {
            continue;
        }

        const float remainingX = toX - position[0];
        const float remainingZ = toZ - position[1];
        const float progressSquared =
            (toX - fromX) * (toX - fromX) +
            (toZ - fromZ) * (toZ - fromZ) -
            (remainingX * remainingX + remainingZ * remainingZ);
        if (!found || progressSquared > best.progressSquared) {
            best = { position[0], position[1], candidateHeight, progressSquared };
            found = true;
        }
    }

    if (!found || best.progressSquared <= 0.0f) return false;
    finalX = best.x;
    finalZ = best.z;
    finalHeight = best.height;
    return true;
}

bool LevelCollisionMesh::FindNearestGroundPoint(float x, float z, float& groundX,
                                                float& groundZ, float& height,
                                                float supportRadius,
                                                float bodyHeight) const {
    if (!std::isfinite(x) || !std::isfinite(z) ||
        !std::isfinite(supportRadius) || !std::isfinite(bodyHeight) ||
        supportRadius < 0.0f || bodyHeight <= 0.0f) {
        return false;
    }

    bool found = false;
    float nearestDistanceSquared = std::numeric_limits<float>::infinity();
    float nearestX = 0.0f;
    float nearestZ = 0.0f;
    float nearestHeight = 0.0f;

    for (const Triangle& triangle : m_triangles) {
        const float candidateX =
            (triangle.a.x + triangle.b.x + triangle.c.x) / 3.0f;
        const float candidateZ =
            (triangle.a.z + triangle.b.z + triangle.c.z) / 3.0f;
        float candidateHeight = 0.0f;
        if (!FindGroundHeight(candidateX, candidateZ, candidateHeight)) continue;
        if (!FindGroundHeightForFootprint(candidateX, candidateZ, candidateHeight,
                                          0.35f, 0.5f, supportRadius,
                                          candidateHeight)) {
            continue;
        }
        if (IsBodyBlocked(candidateX, candidateZ, candidateHeight,
                          supportRadius, bodyHeight)) {
            continue;
        }

        const float dx = candidateX - x;
        const float dz = candidateZ - z;
        const float distanceSquared = dx * dx + dz * dz;
        if (distanceSquared < nearestDistanceSquared ||
            (distanceSquared == nearestDistanceSquared &&
             candidateHeight > nearestHeight)) {
            nearestDistanceSquared = distanceSquared;
            nearestX = candidateX;
            nearestZ = candidateZ;
            nearestHeight = candidateHeight;
            found = true;
        }
    }

    if (!found) return false;
    groundX = nearestX;
    groundZ = nearestZ;
    height = nearestHeight;
    return true;
}

bool LevelCollisionMesh::GetHorizontalBounds(float& minX, float& maxX,
                                             float& minZ, float& maxZ) const {
    if (m_triangles.empty()) return false;
    minX = m_minX;
    maxX = m_maxX;
    minZ = m_minZ;
    maxZ = m_maxZ;
    return true;
}

} // namespace AstroPort
