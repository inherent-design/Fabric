#include "fabric/core/ShadowSystem.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <cmath>
#include <gtest/gtest.h>

using namespace fabric;

class ShadowSystemTest : public ::testing::Test {
  protected:
    Camera camera;
    void SetUp() override { camera.setPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f, true); }
};

TEST_F(ShadowSystemTest, DefaultConfigValues) {
    ShadowConfig cfg;
    EXPECT_EQ(cfg.cascadeCount, 3);
    EXPECT_EQ(cfg.cascadeResolution[0], 2048);
    EXPECT_EQ(cfg.cascadeResolution[1], 2048);
    EXPECT_EQ(cfg.cascadeResolution[2], 1024);
    EXPECT_FLOAT_EQ(cfg.cascadeSplitLambda, 0.75f);
    EXPECT_FLOAT_EQ(cfg.maxShadowDistance, 200.0f);
    EXPECT_EQ(cfg.pcfSamples, 4);
    EXPECT_TRUE(cfg.enabled);
}

TEST_F(ShadowSystemTest, PresetLow) {
    auto cfg = presetConfig(ShadowQualityPreset::Low);
    EXPECT_EQ(cfg.cascadeCount, 1);
    EXPECT_EQ(cfg.cascadeResolution[0], 1024);
    EXPECT_EQ(cfg.pcfSamples, 0);
}

TEST_F(ShadowSystemTest, PresetMedium) {
    auto cfg = presetConfig(ShadowQualityPreset::Medium);
    EXPECT_EQ(cfg.cascadeCount, 2);
    EXPECT_EQ(cfg.cascadeResolution[0], 1024);
    EXPECT_EQ(cfg.cascadeResolution[1], 512);
    EXPECT_EQ(cfg.pcfSamples, 4);
}

TEST_F(ShadowSystemTest, PresetHigh) {
    auto cfg = presetConfig(ShadowQualityPreset::High);
    EXPECT_EQ(cfg.cascadeCount, 3);
    EXPECT_EQ(cfg.cascadeResolution[0], 2048);
    EXPECT_EQ(cfg.cascadeResolution[1], 2048);
    EXPECT_EQ(cfg.cascadeResolution[2], 1024);
    EXPECT_EQ(cfg.pcfSamples, 4);
}

TEST_F(ShadowSystemTest, PresetUltra) {
    auto cfg = presetConfig(ShadowQualityPreset::Ultra);
    EXPECT_EQ(cfg.cascadeCount, 3);
    EXPECT_EQ(cfg.cascadeResolution[0], 4096);
    EXPECT_EQ(cfg.cascadeResolution[1], 2048);
    EXPECT_EQ(cfg.cascadeResolution[2], 2048);
    EXPECT_EQ(cfg.pcfSamples, 9);
}

TEST_F(ShadowSystemTest, RuntimeReconfiguration) {
    ShadowSystem sys;
    auto newCfg = presetConfig(ShadowQualityPreset::Ultra);
    sys.setConfig(newCfg);
    EXPECT_EQ(sys.config().cascadeCount, 3);
    EXPECT_EQ(sys.config().cascadeResolution[0], 4096);
}

TEST_F(ShadowSystemTest, CascadeSplitsPartitionRange) {
    ShadowConfig cfg;
    cfg.cascadeCount = 3;
    cfg.maxShadowDistance = 200.0f;
    ShadowSystem sys(cfg);

    Transform<float> t;
    camera.updateView(t);
    Vector3<float, Space::World> lightDir(0.0f, -1.0f, 0.5f);
    sys.update(camera, lightDir);

    auto splits = sys.splitDistances();
    EXPECT_FLOAT_EQ(splits[0], camera.nearPlane());
    for (int i = 0; i < cfg.cascadeCount; ++i) {
        EXPECT_LT(splits[i], splits[i + 1]);
    }
    EXPECT_LE(splits[cfg.cascadeCount], cfg.maxShadowDistance + 0.01f);
}

TEST_F(ShadowSystemTest, CascadeCountAffectsNumberOfSplits) {
    ShadowConfig cfg1;
    cfg1.cascadeCount = 1;
    ShadowSystem sys1(cfg1);

    ShadowConfig cfg3;
    cfg3.cascadeCount = 3;
    ShadowSystem sys3(cfg3);

    Transform<float> t;
    camera.updateView(t);
    Vector3<float, Space::World> lightDir(0.0f, -1.0f, 0.0f);

    sys1.update(camera, lightDir);
    sys3.update(camera, lightDir);

    auto s1 = sys1.splitDistances();
    auto s3 = sys3.splitDistances();
    EXPECT_FLOAT_EQ(s1[0], camera.nearPlane());
    EXPECT_GT(s1[1], s1[0]);

    EXPECT_FLOAT_EQ(s3[0], camera.nearPlane());
    for (int i = 0; i < 3; ++i) {
        EXPECT_GT(s3[i + 1], s3[i]);
    }
}

TEST_F(ShadowSystemTest, LightVPMatrixIsOrthographic) {
    ShadowSystem sys;
    Transform<float> t;
    camera.updateView(t);
    Vector3<float, Space::World> lightDir(0.0f, -1.0f, 0.0f);
    sys.update(camera, lightDir);

    auto data = sys.getCascadeData(0);
    const auto& m = data.lightViewProj;
    // Orthographic projection: m[3], m[7], m[11] should be 0 (no perspective)
    EXPECT_FLOAT_EQ(m[3], 0.0f);
    EXPECT_FLOAT_EQ(m[7], 0.0f);
    EXPECT_FLOAT_EQ(m[11], 0.0f);
}

TEST_F(ShadowSystemTest, GetCascadeDataOutOfRangeThrows) {
    ShadowSystem sys;
    EXPECT_THROW(sys.getCascadeData(-1), FabricException);
    EXPECT_THROW(sys.getCascadeData(3), FabricException);
}

TEST_F(ShadowSystemTest, DisabledSystemSkipsUpdate) {
    ShadowConfig cfg;
    cfg.enabled = false;
    ShadowSystem sys(cfg);

    Transform<float> t;
    camera.updateView(t);
    Vector3<float, Space::World> lightDir(0.0f, -1.0f, 0.0f);
    sys.update(camera, lightDir);

    auto splits = sys.splitDistances();
    EXPECT_FLOAT_EQ(splits[0], 0.0f);
}

TEST_F(ShadowSystemTest, PSSMLambdaZeroGivesLinearSplits) {
    ShadowConfig cfg;
    cfg.cascadeCount = 2;
    cfg.cascadeSplitLambda = 0.0f;
    cfg.maxShadowDistance = 100.0f;
    ShadowSystem sys(cfg);

    Transform<float> t;
    camera.updateView(t);
    Vector3<float, Space::World> lightDir(0.0f, -1.0f, 0.0f);
    sys.update(camera, lightDir);

    auto splits = sys.splitDistances();
    float near = camera.nearPlane();
    float range = 100.0f;
    float expectedMid = near + (range - near) * 0.5f;
    EXPECT_NEAR(splits[1], expectedMid, 0.01f);
}

TEST_F(ShadowSystemTest, ViewIdReservation) {
    EXPECT_EQ(ShadowSystem::kShadowViewBase, 240);
    EXPECT_EQ(ShadowSystem::kMaxCascades, 4);
}

TEST_F(ShadowSystemTest, TexelSnapping) {
    ShadowConfig cfg;
    cfg.cascadeCount = 1;
    cfg.cascadeResolution = {1024, 0, 0, 0};
    ShadowSystem shadow(cfg);

    Vector3<float, Space::World> lightDir(0.0f, -1.0f, 0.5f);

    Transform<float> t;
    t.setPosition(Vector3<float, Space::World>(5.3f, 10.0f, 7.8f));
    camera.updateView(t);
    shadow.update(camera, lightDir);
    auto cascade = shadow.getCascadeData(0);

    // Verify translation components are quantized to texel grid.
    // NDC texel size for 1024 resolution = 2.0 / 1024 = 0.001953125
    float ndcTexelSize = 2.0f / 1024.0f;

    // mtx[12] and mtx[13] should be exact multiples of ndcTexelSize
    float snapX = cascade.lightViewProj[12] / ndcTexelSize;
    float snapY = cascade.lightViewProj[13] / ndcTexelSize;

    EXPECT_FLOAT_EQ(snapX, std::floor(snapX));
    EXPECT_FLOAT_EQ(snapY, std::floor(snapY));
}
