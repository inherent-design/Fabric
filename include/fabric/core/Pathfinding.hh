#pragma once

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Rendering.hh"

#include <vector>

namespace fabric {

struct PathNode {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct PathResult {
    std::vector<PathNode> waypoints;
    bool found = false;
    int nodesExpanded = 0;
};

struct PathFollower {
    std::vector<PathNode> waypoints;
    int currentWaypoint = 0;
    float arrivalThreshold = 1.5f;
    bool complete = false;
};

struct PathFollowerComponent {
    PathFollower follower;
};

class Pathfinding {
  public:
    void init();
    void shutdown();

    PathResult findPath(const ChunkedGrid<float>& grid, int sx, int sy, int sz, int gx, int gy, int gz,
                        float threshold = 0.5f, int maxNodes = 4096);

    static bool isWalkable(const ChunkedGrid<float>& grid, int x, int y, int z, float threshold = 0.5f);

    static Vec3f seek(const Vec3f& current, const Vec3f& target, float maxSpeed);
    static Vec3f arrive(const Vec3f& current, const Vec3f& target, float maxSpeed, float slowRadius);
    static void advancePathFollower(PathFollower& follower, const Vec3f& currentPos);

  private:
    static float heuristic(int x, int y, int z, int gx, int gy, int gz);
    static int64_t packCoord(int x, int y, int z);
};

} // namespace fabric
