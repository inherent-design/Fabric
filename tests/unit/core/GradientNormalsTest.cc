#include "recurse/world/GradientNormals.hh"
#include <cmath>
#include <gtest/gtest.h>

using namespace recurse;

class GradientNormalsTest : public ::testing::Test {
  protected:
    ChunkDensityCache cache;
    ChunkedGrid<float> density;

    void fillCache() { cache.build(0, 0, 0, density); }
};

TEST_F(GradientNormalsTest, FlatPlaneNormalsPointUp) {
    // Terrain convention: density = surfaceY - wy (positive below surface, negative above).
    // For cache-local Y, world Y = ly - 1. Use density = 17.0 - ly so gradient_y < 0,
    // negated gradient_y > 0, giving upward-pointing normals.
    for (int lz = -1; lz < K_CACHE_SIZE - 1; ++lz)
        for (int ly = -1; ly < K_CACHE_SIZE - 1; ++ly)
            for (int lx = -1; lx < K_CACHE_SIZE - 1; ++lx)
                density.set(lx, ly, lz, 17.0f - static_cast<float>(ly));

    fillCache();

    // Test at several interior points
    Vec3f n1 = computeNormal(cache, 5.0f, 10.0f, 5.0f);
    EXPECT_NEAR(n1.x, 0.0f, 1e-5f);
    EXPECT_NEAR(n1.y, 1.0f, 1e-5f);
    EXPECT_NEAR(n1.z, 0.0f, 1e-5f);

    Vec3f n2 = computeNormal(cache, 15.0f, 20.0f, 25.0f);
    EXPECT_NEAR(n2.x, 0.0f, 1e-5f);
    EXPECT_NEAR(n2.y, 1.0f, 1e-5f);
    EXPECT_NEAR(n2.z, 0.0f, 1e-5f);
}

TEST_F(GradientNormalsTest, SphereNormalsPointRadially) {
    // Sphere density = R - distance(p, center). Center at cache (17, 17, 17), R = 10.
    float cx = 17.0f, cy = 17.0f, cz = 17.0f, R = 10.0f;
    for (int lz = -1; lz < K_CACHE_SIZE - 1; ++lz)
        for (int ly = -1; ly < K_CACHE_SIZE - 1; ++ly)
            for (int lx = -1; lx < K_CACHE_SIZE - 1; ++lx) {
                // Cache-local coordinate is lx+1, ly+1, lz+1
                float dx = static_cast<float>(lx + 1) - cx;
                float dy = static_cast<float>(ly + 1) - cy;
                float dz = static_cast<float>(lz + 1) - cz;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                density.set(lx, ly, lz, R - dist);
            }

    fillCache();

    // Test normal at a surface point: directly above center (+Y direction)
    // At cache (17, 27, 17) which is R=10 from center along +Y
    Vec3f n = computeNormal(cache, 17.0f, 27.0f, 17.0f);
    EXPECT_NEAR(n.x, 0.0f, 0.05f);
    EXPECT_NEAR(n.y, 1.0f, 0.05f);
    EXPECT_NEAR(n.z, 0.0f, 0.05f);

    // Test at +X direction
    Vec3f nx = computeNormal(cache, 27.0f, 17.0f, 17.0f);
    EXPECT_NEAR(nx.x, 1.0f, 0.05f);
    EXPECT_NEAR(nx.y, 0.0f, 0.05f);
    EXPECT_NEAR(nx.z, 0.0f, 0.05f);

    // Test at +Z direction
    Vec3f nz = computeNormal(cache, 17.0f, 17.0f, 27.0f);
    EXPECT_NEAR(nz.x, 0.0f, 0.05f);
    EXPECT_NEAR(nz.y, 0.0f, 0.05f);
    EXPECT_NEAR(nz.z, 1.0f, 0.05f);
}

TEST_F(GradientNormalsTest, NormalsAreUnitLength) {
    // Fill with some non-trivial density field
    for (int lz = -1; lz < K_CACHE_SIZE - 1; ++lz)
        for (int ly = -1; ly < K_CACHE_SIZE - 1; ++ly)
            for (int lx = -1; lx < K_CACHE_SIZE - 1; ++lx) {
                float val = std::sin(static_cast<float>(lx) * 0.3f) +
                            std::cos(static_cast<float>(ly) * 0.2f) * static_cast<float>(lz) * 0.1f;
                density.set(lx, ly, lz, val);
            }

    fillCache();

    // Sample normals at various points and verify unit length
    float testPoints[][3] = {
        {5.0f, 5.0f, 5.0f},
        {10.5f, 15.3f, 20.7f},
        {1.0f, 1.0f, 1.0f},
        {30.0f, 30.0f, 30.0f},
    };

    for (const auto& p : testPoints) {
        Vec3f n = computeNormal(cache, p[0], p[1], p[2]);
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        EXPECT_NEAR(len, 1.0f, 1e-5f);
    }
}

TEST_F(GradientNormalsTest, DegenerateFlatFieldReturnsUp) {
    // Uniform density -> zero gradient -> fallback to (0, 1, 0)
    for (int lz = -1; lz < K_CACHE_SIZE - 1; ++lz)
        for (int ly = -1; ly < K_CACHE_SIZE - 1; ++ly)
            for (int lx = -1; lx < K_CACHE_SIZE - 1; ++lx)
                density.set(lx, ly, lz, 5.0f);

    fillCache();

    Vec3f n = computeNormal(cache, 16.0f, 16.0f, 16.0f);
    EXPECT_NEAR(n.x, 0.0f, 1e-5f);
    EXPECT_NEAR(n.y, 1.0f, 1e-5f);
    EXPECT_NEAR(n.z, 0.0f, 1e-5f);
}
