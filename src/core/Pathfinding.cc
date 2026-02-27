#include "fabric/core/Pathfinding.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <unordered_map>
#include <vector>

namespace fabric {

void Pathfinding::init() {}

void Pathfinding::shutdown() {}

bool Pathfinding::isWalkable(const ChunkedGrid<float>& grid, int x, int y, int z, float threshold) {
    return grid.get(x, y, z) < threshold;
}

int64_t Pathfinding::packCoord(int x, int y, int z) {
    return (static_cast<int64_t>(x) << 42) | (static_cast<int64_t>(y & 0x1FFFFF) << 21) |
           static_cast<int64_t>(z & 0x1FFFFF);
}

float Pathfinding::heuristic(int x, int y, int z, int gx, int gy, int gz) {
    return static_cast<float>(std::abs(x - gx) + std::abs(y - gy) + std::abs(z - gz));
}

PathResult Pathfinding::findPath(const ChunkedGrid<float>& grid, int sx, int sy, int sz, int gx, int gy, int gz,
                                 float threshold, int maxNodes) {
    PathResult result;

    if (!isWalkable(grid, sx, sy, sz, threshold) || !isWalkable(grid, gx, gy, gz, threshold)) {
        return result;
    }

    if (sx == gx && sy == gy && sz == gz) {
        result.found = true;
        result.waypoints.push_back({sx, sy, sz});
        return result;
    }

    struct AStarNode {
        int x, y, z;
        float g, f;
        int parentIdx;
    };

    auto cmp = [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
        return a.first > b.first;
    };
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, decltype(cmp)> open(cmp);

    std::vector<AStarNode> nodes;
    std::unordered_map<int64_t, int> visited;

    constexpr std::array<std::array<int, 3>, 6> dirs = {
        {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

    float h = heuristic(sx, sy, sz, gx, gy, gz);
    nodes.push_back({sx, sy, sz, 0.0f, h, -1});
    open.push({h, 0});
    visited[packCoord(sx, sy, sz)] = 0;

    while (!open.empty()) {
        auto [fCost, idx] = open.top();
        open.pop();

        const auto cur = nodes[idx];

        if (cur.x == gx && cur.y == gy && cur.z == gz) {
            result.found = true;
            int i = idx;
            while (i >= 0) {
                result.waypoints.push_back({nodes[i].x, nodes[i].y, nodes[i].z});
                i = nodes[i].parentIdx;
            }
            std::reverse(result.waypoints.begin(), result.waypoints.end());
            return result;
        }

        if (result.nodesExpanded >= maxNodes) {
            return result;
        }
        ++result.nodesExpanded;

        if (cur.f > fCost + 1e-5f) {
            continue;
        }

        for (const auto& d : dirs) {
            int nx = cur.x + d[0];
            int ny = cur.y + d[1];
            int nz = cur.z + d[2];

            if (!isWalkable(grid, nx, ny, nz, threshold)) {
                continue;
            }

            float ng = cur.g + 1.0f;
            int64_t key = packCoord(nx, ny, nz);
            auto it = visited.find(key);

            if (it != visited.end()) {
                if (nodes[it->second].g <= ng) {
                    continue;
                }
                nodes[it->second].g = ng;
                nodes[it->second].f = ng + heuristic(nx, ny, nz, gx, gy, gz);
                nodes[it->second].parentIdx = idx;
                open.push({nodes[it->second].f, it->second});
            } else {
                float nh = heuristic(nx, ny, nz, gx, gy, gz);
                int newIdx = static_cast<int>(nodes.size());
                nodes.push_back({nx, ny, nz, ng, ng + nh, idx});
                visited[key] = newIdx;
                open.push({ng + nh, newIdx});
            }
        }
    }

    return result;
}

} // namespace fabric
