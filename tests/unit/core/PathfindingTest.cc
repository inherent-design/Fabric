#include "fabric/core/Pathfinding.hh"

#include <cmath>

#include <gtest/gtest.h>

using namespace fabric;

namespace {

bool areAdjacent(const PathNode& a, const PathNode& b) {
    int dx = std::abs(a.x - b.x);
    int dy = std::abs(a.y - b.y);
    int dz = std::abs(a.z - b.z);
    return (dx + dy + dz) == 1;
}

} // namespace

TEST(PathfindingTest, EmptyGridDirectPath) {
    ChunkedGrid<float> grid;
    Pathfinding pf;
    pf.init();

    auto result = pf.findPath(grid, 0, 0, 0, 7, 0, 0);

    EXPECT_TRUE(result.found);
    EXPECT_FALSE(result.waypoints.empty());
    EXPECT_EQ(result.waypoints.front().x, 0);
    EXPECT_EQ(result.waypoints.front().y, 0);
    EXPECT_EQ(result.waypoints.front().z, 0);
    EXPECT_EQ(result.waypoints.back().x, 7);
    EXPECT_EQ(result.waypoints.back().y, 0);
    EXPECT_EQ(result.waypoints.back().z, 0);

    pf.shutdown();
}

TEST(PathfindingTest, BlockedPath) {
    ChunkedGrid<float> grid;
    for (int y = 0; y < 8; ++y)
        for (int z = 0; z < 8; ++z)
            grid.set(4, y, z, 1.0f);
    grid.set(4, 3, 3, 0.0f);

    Pathfinding pf;
    pf.init();

    auto result = pf.findPath(grid, 0, 3, 3, 7, 3, 3);

    EXPECT_TRUE(result.found);
    EXPECT_EQ(result.waypoints.front().x, 0);
    EXPECT_EQ(result.waypoints.back().x, 7);

    bool passedThroughGap = false;
    for (const auto& wp : result.waypoints) {
        if (wp.x == 4 && wp.y == 3 && wp.z == 3) {
            passedThroughGap = true;
        }
    }
    EXPECT_TRUE(passedThroughGap);

    pf.shutdown();
}

TEST(PathfindingTest, NoPathExists) {
    ChunkedGrid<float> grid;
    int gx = 5, gy = 5, gz = 5;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dz = -1; dz <= 1; ++dz)
                if (dx != 0 || dy != 0 || dz != 0)
                    grid.set(gx + dx, gy + dy, gz + dz, 1.0f);

    Pathfinding pf;
    pf.init();

    auto result = pf.findPath(grid, 0, 0, 0, gx, gy, gz);
    EXPECT_FALSE(result.found);

    pf.shutdown();
}

TEST(PathfindingTest, StartEqualsGoal) {
    ChunkedGrid<float> grid;
    Pathfinding pf;
    pf.init();

    auto result = pf.findPath(grid, 3, 3, 3, 3, 3, 3);

    EXPECT_TRUE(result.found);
    ASSERT_EQ(result.waypoints.size(), 1u);
    EXPECT_EQ(result.waypoints[0].x, 3);
    EXPECT_EQ(result.waypoints[0].y, 3);
    EXPECT_EQ(result.waypoints[0].z, 3);

    pf.shutdown();
}

TEST(PathfindingTest, StartOrGoalBlocked) {
    ChunkedGrid<float> grid;
    grid.set(0, 0, 0, 1.0f);

    Pathfinding pf;
    pf.init();

    auto result = pf.findPath(grid, 0, 0, 0, 5, 5, 5);
    EXPECT_FALSE(result.found);
    EXPECT_TRUE(result.waypoints.empty());

    pf.shutdown();
}

TEST(PathfindingTest, MaxNodesBudget) {
    ChunkedGrid<float> grid;
    Pathfinding pf;
    pf.init();

    auto result = pf.findPath(grid, 0, 0, 0, 50, 0, 0, 0.5f, 10);

    EXPECT_LE(result.nodesExpanded, 10);

    pf.shutdown();
}

TEST(PathfindingTest, WalkabilityCheck) {
    ChunkedGrid<float> grid;
    grid.set(1, 1, 1, 1.0f);
    grid.set(2, 2, 2, 0.0f);

    EXPECT_FALSE(Pathfinding::isWalkable(grid, 1, 1, 1, 0.5f));
    EXPECT_TRUE(Pathfinding::isWalkable(grid, 2, 2, 2, 0.5f));
    EXPECT_TRUE(Pathfinding::isWalkable(grid, 9, 9, 9, 0.5f));
}

TEST(PathfindingTest, ThresholdControl) {
    ChunkedGrid<float> grid;
    grid.set(3, 3, 3, 0.3f);

    EXPECT_TRUE(Pathfinding::isWalkable(grid, 3, 3, 3, 0.5f));
    EXPECT_FALSE(Pathfinding::isWalkable(grid, 3, 3, 3, 0.2f));
}

TEST(PathfindingTest, DiagonalAvoidance) {
    ChunkedGrid<float> grid;
    Pathfinding pf;
    pf.init();

    auto result = pf.findPath(grid, 0, 0, 0, 3, 3, 0);

    EXPECT_TRUE(result.found);
    for (size_t i = 1; i < result.waypoints.size(); ++i) {
        EXPECT_TRUE(areAdjacent(result.waypoints[i - 1], result.waypoints[i]))
            << "Waypoints " << i - 1 << " and " << i << " are not face-adjacent";
    }

    pf.shutdown();
}
