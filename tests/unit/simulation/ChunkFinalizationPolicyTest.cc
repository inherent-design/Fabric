#include "recurse/simulation/ChunkFinalization.hh"
#include "recurse/simulation/MaterialRegistry.hh"

#include <array>
#include <gtest/gtest.h>

using namespace recurse::simulation;

TEST(ChunkFinalizationPolicyTest, LiveEditPoliciesMatchCurrentActivationBehavior) {
    auto place = chunkActivationOptionsForCause(ChunkFinalizationCause::LivePlaceEdit);
    EXPECT_EQ(place.targetState, ChunkState::Active);
    EXPECT_FALSE(place.activateAllSubRegions);
    EXPECT_FALSE(place.notifyTargetBoundaryChange);
    EXPECT_EQ(place.neighborInvalidation, NeighborInvalidation::Face);

    auto destroy = chunkActivationOptionsForCause(ChunkFinalizationCause::LiveDestroyEdit);
    EXPECT_EQ(destroy.targetState, std::nullopt);
    EXPECT_FALSE(destroy.activateAllSubRegions);
    EXPECT_TRUE(destroy.notifyTargetBoundaryChange);
    EXPECT_EQ(destroy.neighborInvalidation, NeighborInvalidation::Face);
}

TEST(ChunkFinalizationPolicyTest, ReadyPathPoliciesRemainExplicitPerCause) {
    auto initial = chunkActivationOptionsForCause(ChunkFinalizationCause::InitialWorldGenerationReady);
    EXPECT_EQ(initial.targetState, ChunkState::Active);
    EXPECT_FALSE(initial.activateAllSubRegions);
    EXPECT_FALSE(initial.notifyTargetBoundaryChange);
    EXPECT_EQ(initial.neighborInvalidation, NeighborInvalidation::None);

    auto generated = chunkActivationOptionsForCause(ChunkFinalizationCause::StreamingGenerationReady);
    EXPECT_EQ(generated.targetState, ChunkState::Active);
    EXPECT_EQ(generated.neighborInvalidation, NeighborInvalidation::FaceAndDiagonalXZ);

    auto loaded = chunkActivationOptionsForCause(ChunkFinalizationCause::AsyncLoadReady);
    EXPECT_EQ(loaded.targetState, ChunkState::Active);
    EXPECT_EQ(loaded.neighborInvalidation, NeighborInvalidation::FaceAndDiagonalXZ);

    auto restored = chunkActivationOptionsForCause(ChunkFinalizationCause::ReplayRestoreReady);
    EXPECT_EQ(restored.targetState, ChunkState::Active);
    EXPECT_TRUE(restored.activateAllSubRegions);

    auto replayPlace = chunkActivationOptionsForCause(ChunkFinalizationCause::ReplayPlaceEdit);
    EXPECT_EQ(replayPlace.targetState, ChunkState::Active);
    EXPECT_TRUE(replayPlace.activateAllSubRegions);

    auto replayDestroy = chunkActivationOptionsForCause(ChunkFinalizationCause::ReplayDestroyEdit);
    EXPECT_EQ(replayDestroy.targetState, ChunkState::Active);
    EXPECT_TRUE(replayDestroy.activateAllSubRegions);
}

TEST(ChunkFinalizationPolicyTest, BufferPoliciesPreserveRestoreBehaviorByCause) {
    MaterialRegistry materials;
    const std::array<float, 4> paletteData{1.0f, 2.0f, 3.0f, 4.0f};

    ChunkBufferPolicyInputs loadInputs;
    loadInputs.sourceBufferIndex = 2;
    loadInputs.materials = &materials;
    loadInputs.paletteData = std::span<const float>(paletteData.data(), paletteData.size());

    auto generated =
        chunkBufferFinalizationOptionsForCause(ChunkFinalizationCause::StreamingGenerationReady, loadInputs);
    EXPECT_EQ(generated.sourceBufferIndex, 2);
    EXPECT_FALSE(generated.restorePalette);
    EXPECT_EQ(generated.materials, nullptr);
    EXPECT_TRUE(generated.paletteData.empty());

    auto loaded = chunkBufferFinalizationOptionsForCause(ChunkFinalizationCause::AsyncLoadReady, loadInputs);
    EXPECT_EQ(loaded.sourceBufferIndex, 2);
    EXPECT_TRUE(loaded.restorePalette);
    EXPECT_EQ(loaded.materials, &materials);
    ASSERT_EQ(loaded.paletteData.size(), paletteData.size());
    EXPECT_FLOAT_EQ(loaded.paletteData[0], 1.0f);

    auto restored = chunkBufferFinalizationOptionsForCause(ChunkFinalizationCause::ReplayRestoreReady, loadInputs);
    EXPECT_EQ(restored.sourceBufferIndex, 2);
    EXPECT_TRUE(restored.restorePalette);
    EXPECT_EQ(restored.materials, &materials);
    ASSERT_EQ(restored.paletteData.size(), paletteData.size());
    EXPECT_FLOAT_EQ(restored.paletteData[3], 4.0f);
}
