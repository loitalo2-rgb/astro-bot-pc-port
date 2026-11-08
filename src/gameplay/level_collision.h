#pragma once

#include "../assets/jxmd_parser.h"

#include <cstddef>
#include <vector>

namespace AstroPort {

class LevelCollisionMesh {
public:
    bool Build(const JxmdMesh& mesh, float minimumUpNormal = 0.65f);
    bool FindGroundHeight(float x, float z, float& height) const;
    bool FindGroundHeightNear(float x, float z, float referenceHeight,
                              float maxStepUp, float maxStepDown,
                              float& height) const;
    bool FindGroundHeightForFootprint(float x, float z, float referenceHeight,
                                      float maxStepUp, float maxStepDown,
                                      float supportRadius, float& height) const;
    bool MoveOnGround(float fromX, float fromZ, float toX, float toZ,
                      float startingHeight, float maxStepUp, float maxStepDown,
                      float& finalHeight, float supportRadius = 0.0f,
                      float bodyHeight = 0.75f) const;
    bool MoveOnGroundWithSlide(float fromX, float fromZ, float toX, float toZ,
                               float startingHeight, float maxStepUp,
                               float maxStepDown, float& finalX, float& finalZ,
                               float& finalHeight, float supportRadius = 0.0f,
                               float bodyHeight = 0.75f) const;
    bool FindNearestGroundPoint(float x, float z, float& groundX,
                                float& groundZ, float& height,
                                float supportRadius = 0.0f,
                                float bodyHeight = 0.75f) const;
    bool GetHorizontalBounds(float& minX, float& maxX,
                             float& minZ, float& maxZ) const;
    size_t GetWalkableTriangleCount() const { return m_triangles.size(); }
    size_t GetObstacleTriangleCount() const { return m_obstacles.size(); }

private:
    struct Triangle {
        JxmdVertex a;
        JxmdVertex b;
        JxmdVertex c;
    };

    std::vector<Triangle> m_triangles;
    std::vector<Triangle> m_obstacles;
    bool IsBodyBlocked(float x, float z, float groundHeight,
                       float supportRadius, float bodyHeight) const;
    float m_minX = 0.0f;
    float m_maxX = 0.0f;
    float m_minZ = 0.0f;
    float m_maxZ = 0.0f;
};

} // namespace AstroPort
