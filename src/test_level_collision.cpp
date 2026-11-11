#include "gameplay/level_collision.h"
#include "gameplay/player_movement.h"
#include "assets/asset_loader.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

AstroPort::JxmdVertex Vertex(float x, float y, float z) {
    AstroPort::JxmdVertex vertex;
    vertex.x = x;
    vertex.y = y;
    vertex.z = z;
    return vertex;
}

AstroPort::JxmdMesh StepMesh(float stepHeight) {
    AstroPort::JxmdMesh mesh;
    mesh.valid = true;
    mesh.vertices = {
        Vertex(0.0f, 0.0f, 0.0f), Vertex(1.0f, 0.0f, 0.0f),
        Vertex(0.0f, 0.0f, 1.0f), Vertex(1.0f, 0.0f, 1.0f),
        Vertex(1.0f, stepHeight, 0.0f), Vertex(2.0f, stepHeight, 0.0f),
        Vertex(1.0f, stepHeight, 1.0f), Vertex(2.0f, stepHeight, 1.0f)
    };
    mesh.indices = { 0, 1, 2, 1, 3, 2, 4, 5, 6, 5, 7, 6 };
    return mesh;
}

AstroPort::JxmdMesh WallMesh(float wallBottom, float wallTop) {
    AstroPort::JxmdMesh mesh;
    mesh.valid = true;
    mesh.vertices = {
        Vertex(0.0f, 0.0f, 0.0f), Vertex(2.0f, 0.0f, 0.0f),
        Vertex(0.0f, 0.0f, 2.0f), Vertex(2.0f, 0.0f, 2.0f),
        Vertex(1.0f, wallBottom, 0.0f), Vertex(1.0f, wallTop, 0.0f),
        Vertex(1.0f, wallBottom, 2.0f), Vertex(1.0f, wallTop, 2.0f)
    };
    mesh.indices = { 0, 1, 2, 1, 3, 2, 4, 5, 6, 5, 7, 6 };
    return mesh;
}

bool Near(float actual, float expected) {
    return std::abs(actual - expected) < 0.0001f;
}

} // namespace

int main(int argc, char* argv[]) {
    using namespace AstroPort;

    JxmdMesh mesh;
    mesh.valid = true;
    mesh.vertices = {
        Vertex(0.0f, 0.0f, 0.0f),
        Vertex(2.0f, 0.0f, 0.0f),
        Vertex(0.0f, 0.0f, 2.0f),
        Vertex(0.0f, 2.0f, 0.0f),
        Vertex(2.0f, 2.0f, 0.0f),
        Vertex(0.0f, 2.0f, 2.0f),
        Vertex(0.0f, 0.0f, 0.0f),
        Vertex(0.0f, 2.0f, 0.0f),
        Vertex(0.0f, 0.0f, 2.0f)
    };
    mesh.indices = {
        0, 1, 2,
        3, 5, 4,
        6, 7, 8,
        0, 1, 99
    };

    LevelCollisionMesh collision;
    if (!collision.Build(mesh) || collision.GetWalkableTriangleCount() != 2) {
        std::cerr << "Walkable triangles were not extracted correctly.\n";
        return 1;
    }

    JxmdMesh filteredMesh = mesh;
    filteredMesh.collisionIndices = { 0, 1, 2 };
    filteredMesh.collisionIndicesRestricted = true;
    LevelCollisionMesh filteredCollision;
    if (!filteredCollision.Build(filteredMesh) ||
        filteredCollision.GetWalkableTriangleCount() != 1) {
        std::cerr << "Restricted collision indices were not honored.\n";
        return 2;
    }

    JxmdMesh fallbackOnly = mesh;
    fallbackOnly.collisionDataValid = false;
    fallbackOnly.collisionIndicesRestricted = true;
    LevelCollisionMesh fallbackCollision;
    if (fallbackCollision.Build(fallbackOnly) ||
        fallbackCollision.GetWalkableTriangleCount() != 0) {
        std::cerr << "Procedural-only geometry was accepted as collision.\n";
        return 3;
    }

    float groundHeight = -1.0f;
    if (!collision.FindGroundHeight(0.5f, 0.5f, groundHeight) ||
        !Near(groundHeight, 2.0f)) {
        std::cerr << "Ground query failed to choose the highest surface.\n";
        return 4;
    }

    float spawnX = 0.0f;
    float spawnZ = 0.0f;
    if (!collision.FindNearestGroundPoint(10.0f, 10.0f, spawnX, spawnZ,
                                         groundHeight) ||
        !collision.FindGroundHeight(spawnX, spawnZ, groundHeight)) {
        std::cerr << "Nearest walkable spawn point could not be found.\n";
        return 5;
    }

    if (collision.FindGroundHeight(1.8f, 1.8f, groundHeight) ||
        collision.FindGroundHeight(
            std::numeric_limits<float>::quiet_NaN(), 0.0f, groundHeight)) {
        std::cerr << "Ground query accepted a point outside the surface.\n";
        return 6;
    }

    if (!collision.FindGroundHeightForFootprint(
            0.5f, 0.5f, 2.0f, 0.35f, 0.5f, 0.08f, groundHeight) ||
        collision.FindGroundHeightForFootprint(
            1.95f, 0.05f, 2.0f, 0.35f, 0.5f, 0.08f, groundHeight)) {
        std::cerr << "Footprint support did not distinguish floor from edge.\n";
        return 7;
    }

    float movedHeight = 0.0f;
    if (!collision.MoveOnGround(
            0.25f, 0.25f, 0.75f, 0.25f, 0.0f, 0.35f, 0.5f,
            movedHeight, 0.08f) ||
        !Near(movedHeight, 0.0f)) {
        std::cerr << "Movement on a continuous floor was rejected.\n";
        return 8;
    }

    if (collision.MoveOnGround(
            0.5f, 0.5f, 1.9f, 1.9f, 0.0f, 0.35f, 0.5f,
            movedHeight, 0.08f)) {
        std::cerr << "Movement crossed a gap without walkable support.\n";
        return 9;
    }

    LevelCollisionMesh smallStepCollision;
    const JxmdMesh smallStep = StepMesh(0.2f);
    if (!smallStepCollision.Build(smallStep) ||
        !smallStepCollision.MoveOnGround(
            0.5f, 0.5f, 1.5f, 0.5f, 0.0f, 0.35f, 0.5f,
            movedHeight, 0.04f) ||
        !Near(movedHeight, 0.2f)) {
        std::cerr << "A permitted step was not traversable.\n";
        return 10;
    }

    LevelCollisionMesh highStepCollision;
    const JxmdMesh highStep = StepMesh(0.6f);
    if (!highStepCollision.Build(highStep) ||
        highStepCollision.MoveOnGround(
            0.5f, 0.5f, 1.5f, 0.5f, 0.0f, 0.35f, 0.5f,
            movedHeight, 0.04f)) {
        std::cerr << "A step above the permitted height was traversable.\n";
        return 11;
    }

    LevelCollisionMesh wallCollision;
    const JxmdMesh wall = WallMesh(0.0f, 1.0f);
    if (!wallCollision.Build(wall) ||
        wallCollision.GetObstacleTriangleCount() != 2 ||
        wallCollision.MoveOnGround(
            0.5f, 1.0f, 1.5f, 1.0f, 0.0f, 0.35f, 0.5f,
            movedHeight, 0.08f, 0.75f)) {
        std::cerr << "Movement crossed a wall intersecting the player body.\n";
        return 12;
    }

    if (!wallCollision.MoveOnGround(
            0.5f, 0.5f, 0.5f, 1.5f, 0.0f, 0.35f, 0.5f,
            movedHeight, 0.08f, 0.75f)) {
        std::cerr << "Movement parallel to a distant wall was blocked.\n";
        return 13;
    }

    float slidX = 0.0f;
    float slidZ = 0.0f;
    if (!wallCollision.MoveOnGroundWithSlide(
            0.5f, 0.5f, 1.5f, 1.5f, 0.0f, 0.35f, 0.5f,
            slidX, slidZ, movedHeight, 0.08f, 0.75f) ||
        !Near(slidX, 0.5f) || !Near(slidZ, 1.5f)) {
        std::cerr << "Diagonal movement did not slide along the wall.\n";
        return 14;
    }

    float clearSpawnX = 0.0f;
    float clearSpawnZ = 0.0f;
    float clearSpawnY = 0.0f;
    if (!wallCollision.FindNearestGroundPoint(
            1.0f, 1.0f, clearSpawnX, clearSpawnZ, clearSpawnY,
            0.08f, 0.75f) ||
        !wallCollision.MoveOnGround(
            clearSpawnX, clearSpawnZ, clearSpawnX, clearSpawnZ,
            clearSpawnY, 0.35f, 0.5f, movedHeight, 0.08f, 0.75f)) {
        std::cerr << "Spawn selection did not find a clear position.\n";
        return 15;
    }

    LevelCollisionMesh overheadCollision;
    const JxmdMesh overheadWall = WallMesh(1.2f, 1.8f);
    if (!overheadCollision.Build(overheadWall)) {
        std::cerr << "The overhead-wall fixture has no walkable floor.\n";
        return 16;
    }
    if (!overheadCollision.FindGroundHeightForFootprint(
            1.0f, 1.0f, 0.0f, 0.35f, 0.5f, 0.08f, groundHeight)) {
        std::cerr << "The overhead-wall fixture lost floor support.\n";
        return 17;
    }
    if (!overheadCollision.MoveOnGround(
            0.5f, 1.0f, 1.5f, 1.0f, 0.0f, 0.35f, 0.5f,
            movedHeight, 0.08f, 0.75f)) {
        std::cerr << "An obstacle above the player blocked movement.\n";
        return 18;
    }

    LevelCollisionMesh deepDropCollision;
    const JxmdMesh deepDrop = StepMesh(-0.6f);
    if (!deepDropCollision.Build(deepDrop) ||
        deepDropCollision.MoveOnGround(
            0.5f, 0.5f, 1.5f, 0.5f, 0.0f, 0.35f, 0.5f,
            movedHeight, 0.04f)) {
        std::cerr << "A drop below the permitted height was traversable.\n";
        return 19;
    }

    JxmdMesh invalid;
    if (collision.Build(invalid) || collision.GetWalkableTriangleCount() != 0) {
        std::cerr << "Invalid mesh left stale collision data.\n";
        return 20;
    }

    if (argc > 1) {
        AssetLoader& loader = AssetLoader::Get();
        JxmdMesh level;
        if (!loader.Initialize(argv[1]) ||
            !loader.LoadLevelMesh("hub_crashsite", level)) {
            std::cerr << "Could not load the real hub mesh from the provided data root.\n";
            return 21;
        }

        LevelCollisionMesh levelCollision;
        if (!levelCollision.Build(level) ||
            levelCollision.GetWalkableTriangleCount() == 0) {
            std::cerr << "The real hub did not produce walkable collision triangles.\n";
            return 22;
        }
        if (level.sourceChunkCount == 0 ||
            level.collisionIndices.empty() ||
            !level.collisionIndicesRestricted) {
            std::cerr << "The real hub collision contains no explicitly sourced geometry.\n";
            return 23;
        }
        if (level.sourceChunkCount != 2 || level.proceduralChunkCount != 6 ||
            level.vertices.size() > 500) {
            std::cerr << "Procedural fallback geometry leaked into the real hub mesh.\n";
            return 24;
        }
        if (level.indices.size() % 3 != 0 ||
            std::any_of(level.indices.begin(), level.indices.end(),
                [&level](uint32_t index) {
                    return index >= level.vertices.size();
                })) {
            std::cerr << "The aggregated hub render mesh contains invalid indices.\n";
            return 25;
        }

        const JxmdMesh* playerMesh = loader.GetCachedAstroMesh();
        if (!playerMesh || !playerMesh->valid) {
            std::cerr << "Could not load the player mesh for collision sizing.\n";
            return 26;
        }
        const float playerExtent = std::max(
            playerMesh->bboxMax[0] - playerMesh->bboxMin[0],
            std::max(playerMesh->bboxMax[1] - playerMesh->bboxMin[1],
                     playerMesh->bboxMax[2] - playerMesh->bboxMin[2]));
        const float levelExtent = std::max(
            level.bboxMax[0] - level.bboxMin[0],
            std::max(level.bboxMax[1] - level.bboxMin[1],
                     level.bboxMax[2] - level.bboxMin[2]));
        const float playerScale = (levelExtent * 0.06f) / playerExtent;
        const float playerBodyHeight =
            (playerMesh->bboxMax[1] - playerMesh->bboxMin[1]) * playerScale;

        const float centerX = (level.bboxMin[0] + level.bboxMax[0]) * 0.5f;
        const float centerZ = (level.bboxMin[2] + level.bboxMax[2]) * 0.5f;
        float realSpawnX = 0.0f;
        float realSpawnZ = 0.0f;
        float realGroundY = 0.0f;
        if (!levelCollision.FindNearestGroundPoint(
                centerX, centerZ, realSpawnX, realSpawnZ, realGroundY,
                0.08f, playerBodyHeight)) {
            std::cerr << "Could not find a spawn point on the real hub mesh.\n";
            return 27;
        }
        float spawnCheckHeight = 0.0f;
        const bool spawnClear = levelCollision.MoveOnGround(
            realSpawnX, realSpawnZ, realSpawnX, realSpawnZ,
            realGroundY, 0.35f, 0.5f, spawnCheckHeight,
            0.08f, playerBodyHeight);
        if (!spawnClear) {
            std::cerr << "The selected hub spawn intersects a blocking triangle.\n";
            return 28;
        }

        float minCollisionX = 0.0f;
        float maxCollisionX = 0.0f;
        float minCollisionZ = 0.0f;
        float maxCollisionZ = 0.0f;
        if (!levelCollision.GetHorizontalBounds(
                minCollisionX, maxCollisionX, minCollisionZ, maxCollisionZ)) {
            std::cerr << "Could not get real hub collision bounds.\n";
            return 29;
        }
        const float moveRange =
            (maxCollisionX - minCollisionX) * 0.18f /
            kPlayerHorizontalLimit;
        const float moveRangeZ =
            (maxCollisionZ - minCollisionZ) * 0.18f /
            kPlayerHorizontalLimit;
        size_t testedGridSamples = 0;
        size_t supportedSamples = 0;
        for (int i = 0; i <= 40; ++i) {
            const float normalizedX = -kPlayerHorizontalLimit +
                (2.0f * kPlayerHorizontalLimit * i / 40.0f);
            float sampleHeight = 0.0f;
            if (levelCollision.FindGroundHeight(
                    realSpawnX + normalizedX * moveRange, realSpawnZ,
                    sampleHeight)) {
                ++supportedSamples;
            }
        }
        if (supportedSamples == 0) {
            std::cerr << "The real hub has no supported horizontal movement samples.\n";
            return 30;
        }

        size_t supportedGridSamples = 0;
        for (int ix = 0; ix <= 20; ++ix) {
            for (int iz = 0; iz <= 20; ++iz) {
                const float normalizedX = -kPlayerHorizontalLimit +
                    (2.0f * kPlayerHorizontalLimit * ix / 20.0f);
                const float normalizedZ = -kPlayerHorizontalLimit +
                    (2.0f * kPlayerHorizontalLimit * iz / 20.0f);
                if (normalizedX * normalizedX + normalizedZ * normalizedZ >
                    kPlayerHorizontalLimit * kPlayerHorizontalLimit) {
                    continue;
                }
                ++testedGridSamples;

                float gridHeight = realGroundY;
                float movedX = 0.0f;
                float movedZ = 0.0f;
                if (levelCollision.MoveOnGroundWithSlide(
                        realSpawnX, realSpawnZ,
                        realSpawnX + normalizedX * moveRange,
                        realSpawnZ - normalizedZ * moveRangeZ,
                        realGroundY, 0.35f, 0.5f, movedX, movedZ,
                        gridHeight, 0.08f, playerBodyHeight)) {
                    ++supportedGridSamples;
                }
            }
        }

        std::cout << "Real hub collision: "
                  << levelCollision.GetWalkableTriangleCount()
                  << " walkable and " << levelCollision.GetObstacleTriangleCount()
                  << " blocking triangles; " << level.sourceChunkCount
                  << " source chunks (" << level.proceduralChunkCount
                  << " procedural chunks excluded), spawn=(" << realSpawnX << ", "
                  << realGroundY << ", " << realSpawnZ << "), spawn clear="
                  << (spawnClear ? "yes" : "no") << ", supported movement samples="
                  << supportedSamples << "/41 on X, planar reachable samples="
                  << supportedGridSamples << "/"
                  << testedGridSamples << " in the tested circular grid.\n";
    }

    return 0;
}
