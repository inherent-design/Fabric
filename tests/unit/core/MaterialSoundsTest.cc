#include "fabric/core/MaterialSounds.hh"

#include <gtest/gtest.h>
#include <set>

using namespace fabric;

// Convenience alias
using Ess = Vector4<float, Space::World>;

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

TEST(MaterialSoundsTest, RegisterMaterialStoresSoundSet) {
    MaterialSounds ms(42);

    MaterialSoundSet stoneSet;
    stoneSet.footstepSounds = {"stone1.wav", "stone2.wav", "stone3.wav"};
    stoneSet.impactSounds = {"stone_hit1.wav", "stone_hit2.wav"};
    ms.registerMaterial(MaterialType::Stone, stoneSet);

    // Verify we can retrieve sounds from the registered set
    std::string foot = ms.getFootstepSound(MaterialType::Stone);
    EXPECT_FALSE(foot.empty());

    std::string impact = ms.getImpactSound(MaterialType::Stone);
    EXPECT_FALSE(impact.empty());
}

// ---------------------------------------------------------------------------
// Essence -> Material classification
// ---------------------------------------------------------------------------

TEST(MaterialSoundsTest, MapEssenceToMaterialGrass) {
    // Green-dominant: g > 0.5, g > r, g > b
    Ess green(0.2f, 0.8f, 0.1f, 1.0f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(green), MaterialType::Grass);
}

TEST(MaterialSoundsTest, MapEssenceToMaterialStone) {
    // Dark gray
    Ess gray(0.3f, 0.3f, 0.3f, 0.5f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(gray), MaterialType::Stone);
}

TEST(MaterialSoundsTest, MapEssenceToMaterialDirt) {
    // Brown: r > 0.4, g > 0.2, b < 0.2, r > g
    Ess brown(0.6f, 0.3f, 0.1f, 1.0f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(brown), MaterialType::Dirt);
}

TEST(MaterialSoundsTest, MapEssenceToMaterialWater) {
    // Blue-dominant: b > 0.6, b > r, b > g
    Ess blue(0.1f, 0.2f, 0.9f, 1.0f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(blue), MaterialType::Water);
}

TEST(MaterialSoundsTest, MapEssenceToMaterialSnow) {
    // Bright white: r > 0.8, g > 0.8, b > 0.8
    Ess white(0.95f, 0.95f, 0.95f, 1.0f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(white), MaterialType::Snow);
}

TEST(MaterialSoundsTest, MapEssenceToMaterialMetal) {
    // High alpha, gray channels close together
    Ess metal(0.5f, 0.5f, 0.5f, 0.9f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(metal), MaterialType::Metal);
}

TEST(MaterialSoundsTest, MapEssenceToMaterialSand) {
    // Warm yellow: r > 0.6, g > 0.5, b < 0.3
    Ess sand(0.8f, 0.7f, 0.2f, 1.0f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(sand), MaterialType::Sand);
}

TEST(MaterialSoundsTest, MapEssenceToMaterialWood) {
    // Brown-green: r in [0.3, 0.7], g in [0.2, 0.5], b < 0.2, r <= g (so Dirt doesn't match)
    Ess wood(0.35f, 0.4f, 0.1f, 0.5f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(wood), MaterialType::Wood);
}

TEST(MaterialSoundsTest, MapEssenceToMaterialDefaultForUnknown) {
    // Something that doesn't match any classifier (e.g., bright magenta)
    Ess unknown(0.9f, 0.1f, 0.9f, 0.3f);
    EXPECT_EQ(MaterialSounds::mapEssenceToMaterial(unknown), MaterialType::Default);
}

// ---------------------------------------------------------------------------
// Sound retrieval
// ---------------------------------------------------------------------------

TEST(MaterialSoundsTest, GetFootstepSoundReturnsFromRegisteredSet) {
    MaterialSounds ms(42);

    MaterialSoundSet dirtSet;
    dirtSet.footstepSounds = {"dirt1.wav", "dirt2.wav", "dirt3.wav"};
    dirtSet.impactSounds = {"dirt_hit.wav"};
    ms.registerMaterial(MaterialType::Dirt, dirtSet);

    std::string sound = ms.getFootstepSound(MaterialType::Dirt);
    std::set<std::string> valid(dirtSet.footstepSounds.begin(), dirtSet.footstepSounds.end());
    EXPECT_TRUE(valid.count(sound)) << "Got: " << sound;
}

TEST(MaterialSoundsTest, GetFootstepSoundSingleAlwaysReturnsThat) {
    MaterialSounds ms(42);

    MaterialSoundSet set;
    set.footstepSounds = {"only.wav"};
    ms.registerMaterial(MaterialType::Grass, set);

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(ms.getFootstepSound(MaterialType::Grass), "only.wav");
    }
}

TEST(MaterialSoundsTest, GetFootstepSoundNoConsecutiveRepeats) {
    MaterialSounds ms(42);

    MaterialSoundSet set;
    set.footstepSounds = {"a.wav", "b.wav"};
    ms.registerMaterial(MaterialType::Stone, set);

    std::string prev = ms.getFootstepSound(MaterialType::Stone);
    for (int i = 0; i < 20; ++i) {
        std::string curr = ms.getFootstepSound(MaterialType::Stone);
        EXPECT_NE(curr, prev) << "Consecutive repeat at iteration " << i;
        prev = curr;
    }
}

TEST(MaterialSoundsTest, GetImpactSoundWorks) {
    MaterialSounds ms(42);

    MaterialSoundSet set;
    set.impactSounds = {"metal_hit1.wav", "metal_hit2.wav", "metal_hit3.wav"};
    ms.registerMaterial(MaterialType::Metal, set);

    std::string sound = ms.getImpactSound(MaterialType::Metal);
    std::set<std::string> valid(set.impactSounds.begin(), set.impactSounds.end());
    EXPECT_TRUE(valid.count(sound)) << "Got: " << sound;
}

TEST(MaterialSoundsTest, GetImpactSoundNoConsecutiveRepeats) {
    MaterialSounds ms(42);

    MaterialSoundSet set;
    set.impactSounds = {"x.wav", "y.wav", "z.wav"};
    ms.registerMaterial(MaterialType::Wood, set);

    std::string prev = ms.getImpactSound(MaterialType::Wood);
    for (int i = 0; i < 20; ++i) {
        std::string curr = ms.getImpactSound(MaterialType::Wood);
        EXPECT_NE(curr, prev) << "Consecutive repeat at iteration " << i;
        prev = curr;
    }
}

TEST(MaterialSoundsTest, UnregisteredMaterialReturnsEmptyString) {
    MaterialSounds ms(42);
    // No materials registered
    std::string sound = ms.getFootstepSound(MaterialType::Sand);
    EXPECT_TRUE(sound.empty());
}

// ---------------------------------------------------------------------------
// Surface detection
// ---------------------------------------------------------------------------

TEST(MaterialSoundsTest, DetectSurfaceBelowReturnsMaterial) {
    MaterialSounds ms(42);

    ChunkedGrid<float> density;
    ChunkedGrid<Ess> essence;

    // Place a solid voxel one unit below the query point
    // Query at (5.5, 10.5, 5.5), solid at y=9
    density.set(5, 9, 5, 1.0f);
    // Green essence -> Grass
    essence.set(5, 9, 5, Ess(0.2f, 0.8f, 0.1f, 1.0f));

    MaterialType result = ms.detectSurfaceBelow(density, essence, 5.5f, 10.5f, 5.5f);
    EXPECT_EQ(result, MaterialType::Grass);
}

TEST(MaterialSoundsTest, DetectSurfaceBelowReturnsDefaultWhenNoSolid) {
    MaterialSounds ms(42);

    ChunkedGrid<float> density;
    ChunkedGrid<Ess> essence;

    // No solid voxels anywhere (air column)
    MaterialType result = ms.detectSurfaceBelow(density, essence, 5.5f, 10.5f, 5.5f);
    EXPECT_EQ(result, MaterialType::Default);
}

TEST(MaterialSoundsTest, DetectSurfaceBelowBeyondMaxDistance) {
    MaterialSounds ms(42);

    ChunkedGrid<float> density;
    ChunkedGrid<Ess> essence;

    // Place solid voxel 5 units below (beyond maxDistance of 2.0)
    density.set(5, 5, 5, 1.0f);
    essence.set(5, 5, 5, Ess(0.3f, 0.3f, 0.3f, 0.5f));

    MaterialType result = ms.detectSurfaceBelow(density, essence, 5.5f, 10.5f, 5.5f);
    EXPECT_EQ(result, MaterialType::Default);
}
