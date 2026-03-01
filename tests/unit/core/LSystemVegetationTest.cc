#include "fabric/core/LSystemVegetation.hh"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <set>

using namespace fabric;

// ---------------------------------------------------------------------------
// 1. expand() produces correct string for 1 iteration
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, ExpandOneIteration) {
    LSystemRule rule;
    rule.axiom = "A";
    rule.rules = {{'A', "AB"}, {'B', "A"}};
    rule.iterations = 1;

    std::string result = expand(rule);
    EXPECT_EQ(result, "AB");
}

// ---------------------------------------------------------------------------
// 2. expand() produces correct string for 2 iterations
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, ExpandTwoIterations) {
    LSystemRule rule;
    rule.axiom = "A";
    rule.rules = {{'A', "AB"}, {'B', "A"}};
    rule.iterations = 2;

    // Iter 1: A -> AB
    // Iter 2: A->AB, B->A => ABA
    std::string result = expand(rule);
    EXPECT_EQ(result, "ABA");
}

// ---------------------------------------------------------------------------
// 3. expand() produces correct string for 3 iterations
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, ExpandThreeIterations) {
    LSystemRule rule;
    rule.axiom = "A";
    rule.rules = {{'A', "AB"}, {'B', "A"}};
    rule.iterations = 3;

    // Iter 1: AB
    // Iter 2: ABA
    // Iter 3: A->AB, B->A, A->AB => ABAAB
    std::string result = expand(rule);
    EXPECT_EQ(result, "ABAAB");
}

// ---------------------------------------------------------------------------
// 4. expand() leaves unknown characters unchanged
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, ExpandPreservesUnknownChars) {
    LSystemRule rule;
    rule.axiom = "F+F";
    rule.rules = {{'F', "FF"}};
    rule.iterations = 1;

    std::string result = expand(rule);
    EXPECT_EQ(result, "FF+FF");
}

// ---------------------------------------------------------------------------
// 5. expand() with zero iterations returns axiom unchanged
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, ExpandZeroIterations) {
    LSystemRule rule;
    rule.axiom = "FX";
    rule.rules = {{'F', "FF"}, {'X', "F[+X][-X]"}};
    rule.iterations = 0;

    std::string result = expand(rule);
    EXPECT_EQ(result, "FX");
}

// ---------------------------------------------------------------------------
// 6. interpret() produces non-empty segments for each preset
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, PresetsProduceSegments) {
    {
        auto expanded = expand(kBushRule);
        auto segments = interpret(expanded, kBushRule);
        EXPECT_GT(segments.size(), 0u);
    }
    {
        auto expanded = expand(kSmallTreeRule);
        auto segments = interpret(expanded, kSmallTreeRule);
        EXPECT_GT(segments.size(), 0u);
    }
    {
        auto expanded = expand(kLargeTreeRule);
        auto segments = interpret(expanded, kLargeTreeRule);
        EXPECT_GT(segments.size(), 0u);
    }
}

// ---------------------------------------------------------------------------
// 7. Three presets produce distinct shapes (different segment counts)
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, PresetsProduceDistinctShapes) {
    auto bushSegments = interpret(expand(kBushRule), kBushRule);
    auto smallTreeSegments = interpret(expand(kSmallTreeRule), kSmallTreeRule);
    auto largeTreeSegments = interpret(expand(kLargeTreeRule), kLargeTreeRule);

    std::set<size_t> counts;
    counts.insert(bushSegments.size());
    counts.insert(smallTreeSegments.size());
    counts.insert(largeTreeSegments.size());

    // All three should have distinct segment counts.
    EXPECT_EQ(counts.size(), 3u);
}

// ---------------------------------------------------------------------------
// 8. Segments have positive radii that decay along branches
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, RadiiPositiveAndDecay) {
    LSystemRule rule;
    rule.axiom = "F[F[F]]";
    rule.rules = {};
    rule.iterations = 0;
    rule.angle = 25.0f;
    rule.segmentLength = 1.0f;
    rule.radiusDecay = 0.5f;

    auto segments = interpret(expand(rule), rule);

    // Should have 3 segments: trunk F, branch F (after [), inner F (after [[).
    ASSERT_EQ(segments.size(), 3u);

    // All radii must be positive.
    for (const auto& seg : segments) {
        EXPECT_GT(seg.radius, 0.0f);
    }

    // Radius should decay: seg[0].radius > seg[1].radius > seg[2].radius
    EXPECT_GT(segments[0].radius, segments[1].radius);
    EXPECT_GT(segments[1].radius, segments[2].radius);
}

// ---------------------------------------------------------------------------
// 9. Push/pop correctness: positions return to saved state
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, PushPopRestoresPosition) {
    LSystemRule rule;
    rule.axiom = "F[F]F";
    rule.rules = {};
    rule.iterations = 0;
    rule.angle = 25.0f;
    rule.segmentLength = 1.0f;
    rule.radiusDecay = 0.7f;

    auto segments = interpret(expand(rule), rule);

    // 3 segments:
    //   seg[0]: trunk F (0,0,0) -> (0,1,0)
    //   seg[1]: branch F inside [] (0,1,0) -> (0,2,0)
    //   seg[2]: after pop, F from (0,1,0) -> (0,2,0)

    ASSERT_EQ(segments.size(), 3u);

    // After ] pop, the third segment should start from the same position
    // as the second segment started (the pushed position).
    EXPECT_NEAR(segments[2].start.x, segments[1].start.x, 1e-5f);
    EXPECT_NEAR(segments[2].start.y, segments[1].start.y, 1e-5f);
    EXPECT_NEAR(segments[2].start.z, segments[1].start.z, 1e-5f);
}

// ---------------------------------------------------------------------------
// 10. Deterministic: same input always produces same output
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, DeterministicOutput) {
    auto expanded1 = expand(kSmallTreeRule);
    auto expanded2 = expand(kSmallTreeRule);
    EXPECT_EQ(expanded1, expanded2);

    auto segments1 = interpret(expanded1, kSmallTreeRule);
    auto segments2 = interpret(expanded2, kSmallTreeRule);

    ASSERT_EQ(segments1.size(), segments2.size());

    for (size_t i = 0; i < segments1.size(); ++i) {
        EXPECT_FLOAT_EQ(segments1[i].start.x, segments2[i].start.x);
        EXPECT_FLOAT_EQ(segments1[i].start.y, segments2[i].start.y);
        EXPECT_FLOAT_EQ(segments1[i].start.z, segments2[i].start.z);
        EXPECT_FLOAT_EQ(segments1[i].end.x, segments2[i].end.x);
        EXPECT_FLOAT_EQ(segments1[i].end.y, segments2[i].end.y);
        EXPECT_FLOAT_EQ(segments1[i].end.z, segments2[i].end.z);
        EXPECT_FLOAT_EQ(segments1[i].radius, segments2[i].radius);
        EXPECT_EQ(segments1[i].materialTag, segments2[i].materialTag);
    }
}

// ---------------------------------------------------------------------------
// 11. Leaf marker switches material tag
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, LeafMarkerSwitchesMaterial) {
    LSystemRule rule;
    rule.axiom = "FLF";
    rule.rules = {};
    rule.iterations = 0;
    rule.angle = 25.0f;
    rule.segmentLength = 1.0f;
    rule.radiusDecay = 0.7f;

    auto segments = interpret(expand(rule), rule);

    ASSERT_EQ(segments.size(), 2u);

    // First segment: wood (0).
    EXPECT_EQ(segments[0].materialTag, 0);
    // Second segment: after L, should be leaf (1).
    EXPECT_EQ(segments[1].materialTag, 1);
}

// ---------------------------------------------------------------------------
// 12. Forward without segment ('f') moves position but creates no segment
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, LowercaseFNoSegment) {
    LSystemRule rule;
    rule.axiom = "FfF";
    rule.rules = {};
    rule.iterations = 0;
    rule.angle = 25.0f;
    rule.segmentLength = 1.0f;
    rule.radiusDecay = 0.7f;

    auto segments = interpret(expand(rule), rule);

    // Only 2 segments from uppercase F, not 3.
    ASSERT_EQ(segments.size(), 2u);

    // Second F segment should start 2 units up (F moved 1, f moved 1 more).
    EXPECT_NEAR(segments[1].start.y, 2.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// 13. Yaw produces non-collinear segments
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, YawProducesNonCollinearSegments) {
    LSystemRule rule;
    rule.axiom = "F+F";
    rule.rules = {};
    rule.iterations = 0;
    rule.angle = 90.0f;
    rule.segmentLength = 1.0f;
    rule.radiusDecay = 0.7f;

    auto segments = interpret(expand(rule), rule);

    ASSERT_EQ(segments.size(), 2u);

    // First segment direction should be (0,1,0).
    glm::vec3 dir1 = segments[0].end - segments[0].start;
    // Second segment direction should be rotated 90 degrees around up (Z).
    glm::vec3 dir2 = segments[1].end - segments[1].start;

    // Dot product of perpendicular directions should be ~0.
    float dot = glm::dot(glm::normalize(dir1), glm::normalize(dir2));
    EXPECT_NEAR(dot, 0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// 14. Push/pop also restores radius
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, PushPopRestoresRadius) {
    LSystemRule rule;
    rule.axiom = "F[F]F";
    rule.rules = {};
    rule.iterations = 0;
    rule.angle = 25.0f;
    rule.segmentLength = 1.0f;
    rule.radiusDecay = 0.5f;

    auto segments = interpret(expand(rule), rule);

    ASSERT_EQ(segments.size(), 3u);

    // seg[0] is before push: original radius.
    // seg[1] is inside brackets: decayed radius.
    // seg[2] is after pop: should have original radius restored.
    EXPECT_FLOAT_EQ(segments[0].radius, segments[2].radius);
    EXPECT_GT(segments[0].radius, segments[1].radius);
}

// ===========================================================================
// Voxelization tests (EF-15.2)
// ===========================================================================

// ---------------------------------------------------------------------------
// 15. Single segment produces non-zero density
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, VoxelizeSingleSegmentNonZeroDensity) {
    DensityField density;
    EssenceField essence;

    TurtleSegment seg;
    seg.start = glm::vec3(0.0f, 0.0f, 0.0f);
    seg.end = glm::vec3(5.0f, 0.0f, 0.0f);
    seg.radius = 1.0f;
    seg.materialTag = 0; // wood

    voxelizeSegment(seg, density, essence);

    // At least one voxel along the segment should have non-zero density.
    bool foundNonZero = false;
    for (int x = 0; x <= 5; ++x) {
        if (density.read(x, 0, 0) > 0.0f) {
            foundNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(foundNonZero) << "Voxelization should produce non-zero density along segment";
}

// ---------------------------------------------------------------------------
// 16. Wood vs leaf produce distinct essence values
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, VoxelizeWoodVsLeafDistinctEssence) {
    DensityField densityW, densityL;
    EssenceField essenceW, essenceL;

    TurtleSegment wood;
    wood.start = glm::vec3(0.0f);
    wood.end = glm::vec3(3.0f, 0.0f, 0.0f);
    wood.radius = 1.0f;
    wood.materialTag = 0;

    TurtleSegment leaf;
    leaf.start = glm::vec3(0.0f);
    leaf.end = glm::vec3(3.0f, 0.0f, 0.0f);
    leaf.radius = 1.0f;
    leaf.materialTag = 1;

    voxelizeSegment(wood, densityW, essenceW);
    voxelizeSegment(leaf, densityL, essenceL);

    auto woodEss = essenceW.read(1, 0, 0);
    auto leafEss = essenceL.read(1, 0, 0);

    // Wood and leaf should map to different essence values.
    bool different =
        (woodEss.x != leafEss.x) || (woodEss.y != leafEss.y) || (woodEss.z != leafEss.z) || (woodEss.w != leafEss.w);
    EXPECT_TRUE(different) << "Wood and leaf essences must be distinct";

    // Verify they match the constants.
    EXPECT_FLOAT_EQ(woodEss.x, kWoodEssence.x);
    EXPECT_FLOAT_EQ(woodEss.y, kWoodEssence.y);
    EXPECT_FLOAT_EQ(leafEss.x, kLeafEssence.x);
    EXPECT_FLOAT_EQ(leafEss.y, kLeafEssence.y);
}

// ---------------------------------------------------------------------------
// 17. Radius controls voxel width
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, VoxelizeRadiusControlsWidth) {
    DensityField densityNarrow, densityWide;
    EssenceField essenceNarrow, essenceWide;

    TurtleSegment narrow;
    narrow.start = glm::vec3(0.0f, 0.0f, 0.0f);
    narrow.end = glm::vec3(10.0f, 0.0f, 0.0f);
    narrow.radius = 1.0f;
    narrow.materialTag = 0;

    TurtleSegment wide;
    wide.start = glm::vec3(0.0f, 0.0f, 0.0f);
    wide.end = glm::vec3(10.0f, 0.0f, 0.0f);
    wide.radius = 3.0f;
    wide.materialTag = 0;

    voxelizeSegment(narrow, densityNarrow, essenceNarrow);
    voxelizeSegment(wide, densityWide, essenceWide);

    // Count non-zero voxels in a cross-section at x=5.
    int narrowCount = 0;
    int wideCount = 0;
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dz = -4; dz <= 4; ++dz) {
            if (densityNarrow.read(5, dy, dz) > 0.0f)
                ++narrowCount;
            if (densityWide.read(5, dy, dz) > 0.0f)
                ++wideCount;
        }
    }

    EXPECT_GT(wideCount, narrowCount) << "Wider radius should produce more voxels in cross-section";
}

// ---------------------------------------------------------------------------
// 18. Density stays clamped to [0, 1]
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, VoxelizeDensityClamped) {
    DensityField density;
    EssenceField essence;

    // Voxelize many overlapping segments to try to exceed 1.0.
    for (int i = 0; i < 10; ++i) {
        TurtleSegment seg;
        seg.start = glm::vec3(0.0f);
        seg.end = glm::vec3(3.0f, 0.0f, 0.0f);
        seg.radius = 1.0f;
        seg.materialTag = 0;
        voxelizeSegment(seg, density, essence);
    }

    // Check all voxels along the segment are within [0, 1].
    for (int x = 0; x <= 3; ++x) {
        float d = density.read(x, 0, 0);
        EXPECT_GE(d, 0.0f) << "Density must be >= 0 at x=" << x;
        EXPECT_LE(d, 1.0f) << "Density must be <= 1 at x=" << x;
    }
}

// ---------------------------------------------------------------------------
// 19. voxelizeTree origin offset works
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, VoxelizeTreeOriginOffset) {
    DensityField density;
    EssenceField essence;

    TurtleSegment seg;
    seg.start = glm::vec3(0.0f, 0.0f, 0.0f);
    seg.end = glm::vec3(0.0f, 5.0f, 0.0f);
    seg.radius = 1.0f;
    seg.materialTag = 0;

    glm::ivec3 origin(100, 200, 300);
    voxelizeTree({seg}, density, essence, origin);

    // Density at the origin-shifted location should be non-zero.
    bool foundAtOffset = false;
    for (int y = 200; y <= 205; ++y) {
        if (density.read(100, y, 300) > 0.0f) {
            foundAtOffset = true;
            break;
        }
    }
    EXPECT_TRUE(foundAtOffset) << "Voxelized tree should appear at origin offset";

    // Original location should remain zero.
    float atZero = density.read(0, 2, 0);
    EXPECT_FLOAT_EQ(atZero, 0.0f) << "Original (un-offset) location should be empty";
}

// ---------------------------------------------------------------------------
// 20. Empty segments produce no changes
// ---------------------------------------------------------------------------
TEST(LSystemVegetationTest, VoxelizeEmptySegmentsNoChange) {
    DensityField density;
    EssenceField essence;

    std::vector<TurtleSegment> empty;
    glm::ivec3 origin(0, 0, 0);
    voxelizeTree(empty, density, essence, origin);

    // Grid should have no allocated chunks.
    EXPECT_EQ(density.grid().chunkCount(), 0u) << "Empty segments should allocate no chunks";
    EXPECT_EQ(essence.grid().chunkCount(), 0u) << "Empty segments should allocate no chunks";
}
