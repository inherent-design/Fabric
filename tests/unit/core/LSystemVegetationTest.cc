#include "fabric/core/LSystemVegetation.hh"

#include <gtest/gtest.h>

#include <cmath>
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
