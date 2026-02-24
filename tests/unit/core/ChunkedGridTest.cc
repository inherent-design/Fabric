#include "fabric/core/ChunkedGrid.hh"
#include <gtest/gtest.h>
#include <set>
#include <tuple>
#include <vector>

using namespace fabric;

class ChunkedGridTest : public ::testing::Test {
protected:
    ChunkedGrid<float> grid;
};

TEST_F(ChunkedGridTest, DefaultGetReturnsZero) {
    EXPECT_FLOAT_EQ(grid.get(0, 0, 0), 0.0f);
    EXPECT_FLOAT_EQ(grid.get(100, 200, 300), 0.0f);
}

TEST_F(ChunkedGridTest, SetThenGet) {
    grid.set(5, 10, 15, 42.0f);
    EXPECT_FLOAT_EQ(grid.get(5, 10, 15), 42.0f);
}

TEST_F(ChunkedGridTest, TwoChunksIndependent) {
    grid.set(0, 0, 0, 1.0f);
    grid.set(32, 0, 0, 2.0f);
    EXPECT_EQ(grid.chunkCount(), 2u);
    EXPECT_FLOAT_EQ(grid.get(0, 0, 0), 1.0f);
    EXPECT_FLOAT_EQ(grid.get(32, 0, 0), 2.0f);
}

TEST_F(ChunkedGridTest, CrossChunkBoundary) {
    grid.set(31, 0, 0, 1.0f);
    grid.set(32, 0, 0, 2.0f);
    EXPECT_TRUE(grid.hasChunk(0, 0, 0));
    EXPECT_TRUE(grid.hasChunk(1, 0, 0));
    EXPECT_FLOAT_EQ(grid.get(31, 0, 0), 1.0f);
    EXPECT_FLOAT_EQ(grid.get(32, 0, 0), 2.0f);
}

TEST_F(ChunkedGridTest, Neighbors6Interior) {
    grid.set(10, 10, 10, 1.0f);
    grid.set(11, 10, 10, 2.0f);
    grid.set(9, 10, 10, 3.0f);
    grid.set(10, 11, 10, 4.0f);
    grid.set(10, 9, 10, 5.0f);
    grid.set(10, 10, 11, 6.0f);
    grid.set(10, 10, 9, 7.0f);

    auto n = grid.getNeighbors6(10, 10, 10);
    EXPECT_FLOAT_EQ(n[0], 2.0f); // +x
    EXPECT_FLOAT_EQ(n[1], 3.0f); // -x
    EXPECT_FLOAT_EQ(n[2], 4.0f); // +y
    EXPECT_FLOAT_EQ(n[3], 5.0f); // -y
    EXPECT_FLOAT_EQ(n[4], 6.0f); // +z
    EXPECT_FLOAT_EQ(n[5], 7.0f); // -z
}

TEST_F(ChunkedGridTest, Neighbors6AtChunkBoundary) {
    // Cell at local x=0 in chunk (1,0,0) needs neighbor from chunk (0,0,0)
    grid.set(32, 0, 0, 10.0f);
    grid.set(31, 0, 0, 20.0f); // -x neighbor in chunk (0,0,0)
    grid.set(33, 0, 0, 30.0f); // +x neighbor in chunk (1,0,0)

    auto n = grid.getNeighbors6(32, 0, 0);
    EXPECT_FLOAT_EQ(n[0], 30.0f); // +x
    EXPECT_FLOAT_EQ(n[1], 20.0f); // -x (cross-chunk)
}

TEST_F(ChunkedGridTest, ActiveChunksReturnsCorrectList) {
    grid.set(0, 0, 0, 1.0f);
    grid.set(32, 0, 0, 1.0f);
    grid.set(0, 32, 0, 1.0f);

    auto chunks = grid.activeChunks();
    EXPECT_EQ(chunks.size(), 3u);

    std::set<std::tuple<int, int, int>> chunkSet(chunks.begin(), chunks.end());
    EXPECT_TRUE(chunkSet.count({0, 0, 0}));
    EXPECT_TRUE(chunkSet.count({1, 0, 0}));
    EXPECT_TRUE(chunkSet.count({0, 1, 0}));
}

TEST_F(ChunkedGridTest, RemoveChunkThenGetReturnsZero) {
    grid.set(5, 5, 5, 99.0f);
    EXPECT_FLOAT_EQ(grid.get(5, 5, 5), 99.0f);
    grid.removeChunk(0, 0, 0);
    EXPECT_FLOAT_EQ(grid.get(5, 5, 5), 0.0f);
    EXPECT_FALSE(grid.hasChunk(0, 0, 0));
}

TEST_F(ChunkedGridTest, ForEachCellIteratesFullChunk) {
    grid.set(0, 0, 0, 1.0f); // allocate chunk (0,0,0)
    int count = 0;
    grid.forEachCell(0, 0, 0, [&](int, int, int, float&) { ++count; });
    EXPECT_EQ(count, kChunkVolume);
}

TEST_F(ChunkedGridTest, NegativeCoordinates) {
    grid.set(-1, -1, -1, 77.0f);
    EXPECT_FLOAT_EQ(grid.get(-1, -1, -1), 77.0f);
    EXPECT_TRUE(grid.hasChunk(-1, -1, -1));

    grid.set(-33, 0, 0, 88.0f);
    EXPECT_FLOAT_EQ(grid.get(-33, 0, 0), 88.0f);
    EXPECT_TRUE(grid.hasChunk(-2, 0, 0));
}

TEST_F(ChunkedGridTest, WorldToChunkNegativeFloorDivision) {
    int cx, cy, cz, lx, ly, lz;

    ChunkedGrid<float>::worldToChunk(-1, 0, 0, cx, cy, cz, lx, ly, lz);
    EXPECT_EQ(cx, -1);
    EXPECT_EQ(lx, 31);

    ChunkedGrid<float>::worldToChunk(-32, 0, 0, cx, cy, cz, lx, ly, lz);
    EXPECT_EQ(cx, -1);
    EXPECT_EQ(lx, 0);

    ChunkedGrid<float>::worldToChunk(-33, 0, 0, cx, cy, cz, lx, ly, lz);
    EXPECT_EQ(cx, -2);
    EXPECT_EQ(lx, 31);
}

TEST_F(ChunkedGridTest, ActiveChunksOrderIsDeterministic) {
    // Insert chunks in scattered order
    grid.set(3 * 32, 0, 0, 1.0f);   // chunk (3,0,0)
    grid.set(0, 0, 0, 1.0f);        // chunk (0,0,0)
    grid.set(1 * 32, 0, 0, 1.0f);   // chunk (1,0,0)
    grid.set(-1 * 32, 0, 0, 1.0f);  // chunk (-1,0,0)
    grid.set(0, 2 * 32, 0, 1.0f);   // chunk (0,2,0)

    auto first = grid.activeChunks();
    auto second = grid.activeChunks();
    EXPECT_EQ(first, second);

    // New grid, same chunks inserted in different order
    ChunkedGrid<float> grid2;
    grid2.set(0, 2 * 32, 0, 1.0f);
    grid2.set(-1 * 32, 0, 0, 1.0f);
    grid2.set(0, 0, 0, 1.0f);
    grid2.set(3 * 32, 0, 0, 1.0f);
    grid2.set(1 * 32, 0, 0, 1.0f);

    auto third = grid2.activeChunks();
    EXPECT_EQ(first, third);
}

TEST_F(ChunkedGridTest, IterationOrderMatchesAfterInsertDelete) {
    grid.set(0, 0, 0, 1.0f);
    grid.set(32, 0, 0, 1.0f);
    grid.set(64, 0, 0, 1.0f);
    grid.set(96, 0, 0, 1.0f);
    grid.set(128, 0, 0, 1.0f);

    grid.removeChunk(1, 0, 0);
    grid.removeChunk(3, 0, 0);

    grid.set(-32, 0, 0, 1.0f);
    grid.set(0, 32, 0, 1.0f);
    grid.set(0, 0, 32, 1.0f);

    auto first = grid.activeChunks();

    // Repeat identical operations on a fresh grid in different insert order
    ChunkedGrid<float> grid2;
    grid2.set(128, 0, 0, 1.0f);
    grid2.set(0, 0, 32, 1.0f);
    grid2.set(0, 0, 0, 1.0f);
    grid2.set(64, 0, 0, 1.0f);
    grid2.set(-32, 0, 0, 1.0f);
    grid2.set(0, 32, 0, 1.0f);

    auto second = grid2.activeChunks();
    EXPECT_EQ(first, second);
}

TEST_F(ChunkedGridTest, ForEachChunkDeterministic) {
    grid.set(3 * 32, 0, 0, 1.0f);
    grid.set(0, 0, 0, 1.0f);
    grid.set(-1 * 32, 0, 0, 1.0f);
    grid.set(0, 2 * 32, 0, 1.0f);

    auto active = grid.activeChunks();

    std::vector<std::tuple<int, int, int>> fromForEach;
    grid.forEachChunk([&](int cx, int cy, int cz) { fromForEach.emplace_back(cx, cy, cz); });

    EXPECT_EQ(active, fromForEach);
}
