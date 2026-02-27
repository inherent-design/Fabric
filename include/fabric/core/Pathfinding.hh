#pragma once

#include "fabric/core/ChunkedGrid.hh"

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

class Pathfinding {
  public:
    void init();
    void shutdown();

    PathResult findPath(const ChunkedGrid<float>& grid, int sx, int sy, int sz, int gx, int gy, int gz,
                        float threshold = 0.5f, int maxNodes = 4096);

    static bool isWalkable(const ChunkedGrid<float>& grid, int x, int y, int z, float threshold = 0.5f);

  private:
    static float heuristic(int x, int y, int z, int gx, int gy, int gz);
    static int64_t packCoord(int x, int y, int z);
};

} // namespace fabric
