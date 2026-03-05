#include <gtest/gtest.h>

#include "fabric/core/ConfigManager.hh"
#include "fabric/core/FeatureFlags.hh"

namespace fabric {
namespace test {

// Reset global ConfigManager before/after each test to ensure isolation
class FeatureFlagsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Save original state
        m_originalConfig = nullptr;
        setFeatureFlagConfigManager(nullptr);
    }

    void TearDown() override {
        // Restore to clean state
        setFeatureFlagConfigManager(nullptr);
    }

    const ConfigManager* m_originalConfig;
};

// -- Test 1: DefaultValuesCorrect --

TEST_F(FeatureFlagsTest, DefaultValuesCorrect) {
    // Without ConfigManager, all flags should return their compile-time defaults

    // VP0+ pipeline defaults to enabled
    EXPECT_TRUE(FeatureFlags::voxelSimulation());
    EXPECT_TRUE(FeatureFlags::voxelMeshing());
    EXPECT_TRUE(FeatureFlags::voxelRendering());

    // Legacy defaults to disabled (clean break)
    EXPECT_FALSE(FeatureFlags::legacyDensityField());
    EXPECT_FALSE(FeatureFlags::legacyWaterSim());
    EXPECT_FALSE(FeatureFlags::legacyTerrainGen());

    // Rendering defaults
    EXPECT_TRUE(FeatureFlags::shadows());
    EXPECT_FALSE(FeatureFlags::oit()); // MoltenVK issues per design
    EXPECT_TRUE(FeatureFlags::postProcess());
    EXPECT_TRUE(FeatureFlags::debugDraw());

    // Gameplay defaults
    EXPECT_TRUE(FeatureFlags::physics());
    EXPECT_FALSE(FeatureFlags::audio());     // Experimental
    EXPECT_FALSE(FeatureFlags::particles()); // Experimental
    EXPECT_TRUE(FeatureFlags::characterMovement());
    EXPECT_FALSE(FeatureFlags::behaviorAI()); // Experimental

    // UI defaults
    EXPECT_TRUE(FeatureFlags::rmlUI());
    EXPECT_TRUE(FeatureFlags::debugHUD());
    EXPECT_TRUE(FeatureFlags::waila());
    EXPECT_TRUE(FeatureFlags::devConsole());

    // Debug defaults
    EXPECT_TRUE(FeatureFlags::tracyProfiling());
    EXPECT_FALSE(FeatureFlags::verboseLogging());
}

// -- Test 2: AllFlagsAccessible --

TEST_F(FeatureFlagsTest, AllFlagsAccessible) {
    // Verify all 21 flag methods compile and return bool
    // This catches any API breakage at compile time

    EXPECT_NO_THROW(FeatureFlags::voxelSimulation());
    EXPECT_NO_THROW(FeatureFlags::voxelMeshing());
    EXPECT_NO_THROW(FeatureFlags::voxelRendering());

    EXPECT_NO_THROW(FeatureFlags::legacyDensityField());
    EXPECT_NO_THROW(FeatureFlags::legacyWaterSim());
    EXPECT_NO_THROW(FeatureFlags::legacyTerrainGen());

    EXPECT_NO_THROW(FeatureFlags::shadows());
    EXPECT_NO_THROW(FeatureFlags::oit());
    EXPECT_NO_THROW(FeatureFlags::postProcess());
    EXPECT_NO_THROW(FeatureFlags::debugDraw());

    EXPECT_NO_THROW(FeatureFlags::physics());
    EXPECT_NO_THROW(FeatureFlags::audio());
    EXPECT_NO_THROW(FeatureFlags::particles());
    EXPECT_NO_THROW(FeatureFlags::characterMovement());
    EXPECT_NO_THROW(FeatureFlags::behaviorAI());

    EXPECT_NO_THROW(FeatureFlags::rmlUI());
    EXPECT_NO_THROW(FeatureFlags::debugHUD());
    EXPECT_NO_THROW(FeatureFlags::waila());
    EXPECT_NO_THROW(FeatureFlags::devConsole());

    EXPECT_NO_THROW(FeatureFlags::tracyProfiling());
    EXPECT_NO_THROW(FeatureFlags::verboseLogging());

    // Verify return types are bool
    static_assert(std::is_same_v<decltype(FeatureFlags::voxelSimulation()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::voxelMeshing()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::voxelRendering()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::legacyDensityField()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::legacyWaterSim()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::legacyTerrainGen()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::shadows()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::oit()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::postProcess()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::debugDraw()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::physics()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::audio()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::particles()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::characterMovement()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::behaviorAI()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::rmlUI()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::debugHUD()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::waila()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::devConsole()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::tracyProfiling()), bool>);
    static_assert(std::is_same_v<decltype(FeatureFlags::verboseLogging()), bool>);
}

// -- Test 3: FlagCategoriesWork --

TEST_F(FeatureFlagsTest, FlagCategoriesWork_VP0DefaultsEnabled) {
    // VP0+ should default true (core voxel pipeline)
    EXPECT_TRUE(FeatureFlags::voxelSimulation());
    EXPECT_TRUE(FeatureFlags::voxelMeshing());
    EXPECT_TRUE(FeatureFlags::voxelRendering());
}

TEST_F(FeatureFlagsTest, FlagCategoriesWork_LegacyDefaultsDisabled) {
    // Legacy should default false (clean break design)
    EXPECT_FALSE(FeatureFlags::legacyDensityField());
    EXPECT_FALSE(FeatureFlags::legacyWaterSim());
    EXPECT_FALSE(FeatureFlags::legacyTerrainGen());
}

TEST_F(FeatureFlagsTest, FlagCategoriesWork_RenderingStableFeatures) {
    // Stable rendering features
    EXPECT_TRUE(FeatureFlags::shadows());
    EXPECT_TRUE(FeatureFlags::postProcess());
    EXPECT_TRUE(FeatureFlags::debugDraw());

    // Known problematic feature (MoltenVK issues)
    EXPECT_FALSE(FeatureFlags::oit());
}

TEST_F(FeatureFlagsTest, FlagCategoriesWork_GameplayCoreEnabled) {
    // Core gameplay
    EXPECT_TRUE(FeatureFlags::physics());
    EXPECT_TRUE(FeatureFlags::characterMovement());

    // Experimental features default off
    EXPECT_FALSE(FeatureFlags::audio());
    EXPECT_FALSE(FeatureFlags::particles());
    EXPECT_FALSE(FeatureFlags::behaviorAI());
}

TEST_F(FeatureFlagsTest, FlagCategoriesWork_UIDefaultsEnabled) {
    // UI subsystems default on for development
    EXPECT_TRUE(FeatureFlags::rmlUI());
    EXPECT_TRUE(FeatureFlags::debugHUD());
    EXPECT_TRUE(FeatureFlags::waila());
    EXPECT_TRUE(FeatureFlags::devConsole());
}

TEST_F(FeatureFlagsTest, FlagCategoriesWork_DebugDefaults) {
    // Tracy on by default for profiling
    EXPECT_TRUE(FeatureFlags::tracyProfiling());

    // Verbose logging off by default (noise)
    EXPECT_FALSE(FeatureFlags::verboseLogging());
}

// -- Test 4: ConfigOverrideWorks --

TEST_F(FeatureFlagsTest, ConfigOverrideWorks) {
    ConfigManager config;
    config.set<bool>("features.voxel_simulation", false);
    config.set<bool>("features.legacy_density_field", true);
    config.set<bool>("features.shadows", false);
    config.set<bool>("features.oit", true); // Enable experimental feature

    // Bind ConfigManager to FeatureFlags
    setFeatureFlagConfigManager(&config);

    // Verify overrides take effect
    EXPECT_FALSE(FeatureFlags::voxelSimulation());   // Was true by default
    EXPECT_TRUE(FeatureFlags::legacyDensityField()); // Was false by default
    EXPECT_FALSE(FeatureFlags::shadows());           // Was true by default
    EXPECT_TRUE(FeatureFlags::oit());                // Was false by default

    // Unset flags still use defaults
    EXPECT_TRUE(FeatureFlags::voxelMeshing()); // Default true
    EXPECT_FALSE(FeatureFlags::audio());       // Default false
}

TEST_F(FeatureFlagsTest, ConfigManagerResetRevertsToDefaults) {
    // First, set up config with overrides
    ConfigManager config;
    config.set<bool>("features.voxel_simulation", false);
    setFeatureFlagConfigManager(&config);
    EXPECT_FALSE(FeatureFlags::voxelSimulation());

    // Reset ConfigManager to nullptr
    setFeatureFlagConfigManager(nullptr);

    // Should revert to compile-time default
    EXPECT_TRUE(FeatureFlags::voxelSimulation());
}

TEST_F(FeatureFlagsTest, MissingConfigKeyUsesDefault) {
    ConfigManager config;
    // Don't set any feature flags
    setFeatureFlagConfigManager(&config);

    // All should return defaults since nothing is configured
    EXPECT_TRUE(FeatureFlags::voxelSimulation());
    EXPECT_FALSE(FeatureFlags::legacyDensityField());
    EXPECT_TRUE(FeatureFlags::shadows());
    EXPECT_FALSE(FeatureFlags::oit());
}

// -- Edge cases --

TEST_F(FeatureFlagsTest, AllFlagsReturnBoolNotReference) {
    // Ensure flags return bool values, not references to temporaries
    bool val1 = FeatureFlags::voxelSimulation();
    bool val2 = FeatureFlags::legacyDensityField();
    // These should compile and not cause UB
    (void)val1;
    (void)val2;
}

TEST_F(FeatureFlagsTest, MultipleCallsConsistent) {
    // Multiple calls should return consistent values
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(FeatureFlags::voxelSimulation());
        EXPECT_FALSE(FeatureFlags::legacyDensityField());
    }
}

} // namespace test
} // namespace fabric
